#include "spc_cwc.h"


/* Private prototypes. */
static void cwc_ctr(cwc_t ctx[1], uchar p[], uint32 plen,
		    uchar nonce[11], uchar output[]);
static void cwc_mac(cwc_t ctx[1], uchar a[], uint32 alen, uchar p[],
		    uint32 plen, uchar nonce[11], uchar output[16]);
static void cwc_hash(cwc_t ctx[1], uchar a[], uint32 alen,
		     uchar p[], uint32 plen, uchar output[16]);

static void cwc_memset(volatile void *dst, int c, uint32 len) {
  volatile char *buf = (volatile char *)dst;

  while(len--) buf[len] = c;
}


/* This always takes in a 96-bit input and produces a 128-bit
 * output.
 */
static void cwc_str2int(uchar buf[], uint32 res[4]) {
  res[0] = 0;
  res[1] = htonl(((uint32 *)buf)[0]);
  res[2] = htonl(((uint32 *)buf)[1]);
  res[3] = htonl(((uint32 *)buf)[2]);
}

static void cwc_mod_add(uint32 a[4], uint32 res[4]) {
  int i, carry[4] = {0,};

  for (i=0;i<4;i++) {
    res[i] += a[i];
    if (res[i] < a[i])
      carry[i-1] = 1;
  }
  if (res[0] & 0x80000000) {
    carry[3] = 1;
    res[0] &= 0x7fffffff;
  }
  while (i--) {
    if (carry[i]) {
      res[i] += carry[i];
      if (res[i] < carry[i])
	carry[i-1]++;
    }
  }
}

static void cwc_multiply_128(uint32 a[4], uint32 b[4],
			     uint32 res[8]) {
  int    i, j;
  uint32 upper, lower, carry[8] = {0,};
  uint64 tmp;

  cwc_memset(res, 0, sizeof(uint32)*8);
  for (i=0;i<4;i++) {
    for (j=0;j<4;j++) {
      tmp = (uint64)(a[i]) * (uint64)(b[j]);
      upper = tmp >> 32;
      lower = tmp & 0xffffffff;
      res[i+j]   += upper;
      if (res[i+j] < upper)
	carry[i+j-1]++;
      res[i+j+1] += lower;
      if (res[i+j+1] < lower)
	carry[i+j]++;
    }
  }
  i = 8;
  while (i--) {
    res[i] += carry[i];
    if (carry[i] > res[i])
      carry[i-1]++;
  }
}

static void cwc_mod_256(uint32 v[8]) {
  int i;

  for (i=0;i<4;i++) {
    v[i] <<= 1;
    v[i] |= v[i+1] >> 31;
  }
  v[4] &= 0x7fffffff;
  cwc_mod_add(v,v+4);
}

static void cwc_mod_mul(uint32 a[4], uint32 res[4]) {
  uint32 b[8] = {0,};

  cwc_multiply_128(a, res, b);
  cwc_mod_256(b);
  res[0] = b[4]; res[1] = b[5]; res[2] = b[6]; res[3] = b[7];
}

/* Warning: This zeros out the key before exiting! */
int cwc_init(cwc_t ctx[1], uchar key[], int keybits) {
  uchar  hashkey[16];
  uchar  hash_generator[16] = {0xC0, };  /* all 0s after byte 1.. */

  if (keybits != 128 && keybits != 192 && keybits != 256)
    return 0;
  CWC_AES_SETUP(key, keybits, &ctx->aeskey);
  CWC_AES_ENCRYPT(&ctx->aeskey, hash_generator, hashkey);
  hashkey[0] &= 0x7F;
  ctx->hashkey[0] = htonl(((uint32 *)hashkey)[0]);
  ctx->hashkey[1] = htonl(((uint32 *)hashkey)[1]);
  ctx->hashkey[2] = htonl(((uint32 *)hashkey)[2]);
  ctx->hashkey[3] = htonl(((uint32 *)hashkey)[3]);

  cwc_memset(key, 0, keybits/8);
  return 1;
}

void cwc_encrypt(cwc_t ctx[1], uchar a[], uint32 alen, uchar pt[],
		 uint32 ptlen, uchar nonce[11], uchar output[]) {
  cwc_ctr(ctx, pt, ptlen, nonce, output);
  cwc_mac(ctx, a, alen, output, ptlen, nonce, output+ptlen);
}

int cwc_decrypt(cwc_t ctx[1], uchar a[], uint32 alen, uchar ct[],
		uint32 ctlen, uchar nonce[11], uchar output[]) {
  uchar checktag[16];
  int i;

  if (ctlen < 16)
    return 0;
  cwc_mac(ctx, a, alen, ct, ctlen-16, nonce, checktag);
  for (i=1;i<=16;i++)
    if (ct[ctlen-i] != checktag[16-i])
      return 0;
  cwc_ctr(ctx, ct, ctlen-16, nonce, output);
  return 1;
}

void cwc_cleanup(cwc_t *ctx) {
  cwc_memset((void *)ctx, 0, sizeof(cwc_t));
}

static void cwc_ctr(cwc_t ctx[1], uchar p[], uint32 plen,
		    uchar nonce[11], uchar output[]) {
  uchar  last[16], ctrblk[16] = {0x80,};
  uint32 i, l = plen/16;

  memcpy(ctrblk+1, nonce, 11);
  ctrblk[15] = 0x01;

  for (i=0;i<l;i++) {
    CWC_AES_ENCRYPT(&ctx->aeskey, ctrblk, output);
    ((uint32 *)output)[0] ^= ((uint32 *)p)[0];
    ((uint32 *)output)[1] ^= ((uint32 *)p)[1];
    ((uint32 *)output)[2] ^= ((uint32 *)p)[2];
    ((uint32 *)output)[3] ^= ((uint32 *)p)[3];
    output += 16;
    p      += 16;
    /* On a big endian box we could do one 32 bit increment. */
    if (!++ctrblk[15])
      if (!++ctrblk[14])
	if (!++ctrblk[13])
	  ++ctrblk[12];
  }
  l = plen%16;

  if (l) {
    CWC_AES_ENCRYPT(&ctx->aeskey, ctrblk, last);
    for (i=0;i<l;i++)
      output[i] = last[i] ^ p[i];
  }
}

static void cwc_mac(cwc_t ctx[1], uchar a[], uint32 alen, uchar p[],
		    uint32 plen,  uchar nonce[11], uchar output[16]) {
  uchar hashvalue[16], encrhash[16], ctrblk[16] = {0x80,};
  int i;

  cwc_hash(ctx, a, alen, p, plen, hashvalue);
  CWC_AES_ENCRYPT(&ctx->aeskey, hashvalue, encrhash);
  memcpy(ctrblk+1, nonce, 11);
  CWC_AES_ENCRYPT(&ctx->aeskey, ctrblk, output);
  for (i=0;i<16;i++)
    output[i] ^= encrhash[i];
}

static void cwc_hash(cwc_t ctx[1], uchar a[], uint32 alen, uchar p[],
		     uint32 plen,  uchar out[16]) {
  uint32 i, lo;
  uint32 t[4], res[4] = {0,};
  uchar  padblock[12];

  for (i=0;i<alen/12;i++) {
    cwc_str2int(a+i*12, t);
    cwc_mod_add(t, res);
    cwc_mod_mul(ctx->hashkey, res);
  }
  lo = alen%12;
  if (lo) {
    a = a+i*12;
    for (i=0;i<lo;i++)
      padblock[i] = *a++;
    for (;i<12;i++)
      padblock[i] = 0;
    cwc_str2int(padblock, t);
    cwc_mod_add(t, res);
    cwc_mod_mul(ctx->hashkey, res);
  }
  for (i=0;i<plen/12;i++) {
    cwc_str2int(p+i*12, t);
    cwc_mod_add(t, res);
    cwc_mod_mul(ctx->hashkey, res);
  }
  lo = plen%12;

  if (lo) {
    p = p+i*12;
    for (i=0;i<lo;i++)
      padblock[i] = *p++;
    for (;i<12;i++)
      padblock[i] = 0;
    cwc_str2int(padblock, t);
    cwc_mod_add(t, res);
    cwc_mod_mul(ctx->hashkey, res);
  }
  t[0] = t[2] = 0;
  t[1] = alen;
  t[3] = plen;
  cwc_mod_add(t, res);
  for (i=0;i<4;i++)
    ((uint32 *)out)[i] = htonl(res[i]);
}
