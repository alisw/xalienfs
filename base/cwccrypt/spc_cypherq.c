#include <stdio.h>
#include "spc_cypherq.h"
#include "spc_b64.h"
#include <stdlib.h>
#include <stdio.h>

/*  activate for debugging */
/* #define DEBUG_CWCCRYPT */

void spc_print_cipherq(SPC_CIPHERQ* q) {
  int i;
  printf("PRINTING CIPHERQ (%p)\n  hashkey:",q);
  for(i=0;i<4;i++)
    printf("  %d",q->ctx.hashkey[i]);
  printf("\n");

  printf("  nonce:");
  for(i=0;i< SPC_BLOCK_SZ;i++)
    printf("  %d",q->nonce[i]);
  printf("\n");
}

void spc_cipherq_setnonce(SPC_CIPHERQ* q, unsigned char *nonce)
{
  int i;
  for(i=0;i<SPC_BLOCK_SZ;i++)
    q->nonce[i]=nonce[i];
}

unsigned char *spc_cipherq_getnonce(SPC_CIPHERQ* q)
{
  return(q->nonce);
}

size_t spc_cipherq_setup(SPC_CIPHERQ *q, unsigned char *basekey, size_t keylen,
                         size_t keyuses) {
  unsigned char dk[MAX_KEY_LEN];
  unsigned char salt[5];
  int i;

  spc_rand(salt, 5);
/*   spc_make_derived_key(basekey, keylen, salt, 5, 1, dk, keylen); */
/*   DANGER we currently use the basekey */
  for(i=0;i<=keylen;i++) {
    dk[i]=basekey[i];
  }
  if (!cwc_init(&(q->ctx), dk, keylen * 8)) return 0;
  memcpy(q->nonce, salt, 5);
  spc_memset(basekey, 0, keylen);
  return keyuses + 1;
}

void spc_cipherq_cleanup(SPC_CIPHERQ *q) {
  spc_memset(q, 0, sizeof(SPC_CIPHERQ));
}

static void increment_counter(SPC_CIPHERQ *q) {
  if (!++q->nonce[10]) if (!++q->nonce[9]) if (!++q->nonce[8]) if (!++q->nonce[7])
    if (!++q->nonce[6]) ++q->nonce[5];
}

unsigned char *spc_cipherq_encrypt(SPC_CIPHERQ *q, unsigned char *m, size_t mlen,
                                   size_t *ol) {
  unsigned char *ret;

  if (mlen + 16 < 16) return 0;
  if (!(ret = (unsigned char *)malloc(mlen + 16))) {
    if (ol) *ol = 0;
    return 0;
  }

# ifdef DEBUG_CWCCRYPT
  spc_print_cipherq(q);
# endif
  cwc_encrypt(&(q->ctx), 0, (uint32) 0, m, (uint32) mlen, q->nonce, ret);
  increment_counter(q);
  if (ol) *ol = mlen + 16;
  return ret;
}

/* added +1 byte allocation to '\0' terminate the return value */
unsigned char *spc_cipherq_decrypt(SPC_CIPHERQ *q, unsigned char *m, size_t mlen,
                                   size_t *ol) {
  unsigned char *ret;

  if (mlen < 16) return 0;
  if (!(ret = (unsigned char *)malloc(mlen - 16 +1))) {
    if (ol) *ol = 0;
    return 0;
  }

# ifdef DEBUG_CWCCRYPT
  spc_print_cipherq(q);
# endif

  if (!cwc_decrypt(&(q->ctx), 0, (uint32) 0, m, (uint32) mlen, q->nonce, ret)) {
    free(ret);
    if (ol) *ol = 0;
    return 0;
  }
  increment_counter(q);
  if (ol) *ol = mlen - 16;
  ret[*ol]='\0';
  return ret;
}


/**
   @param q      an initialized SPC_CIPHERQ struct
   @param m      message to encrypt
   @param mlen   length of the message

   @return       the encrypted string
*/
unsigned char *spc_cipherq_b64encrypt(SPC_CIPHERQ *q, unsigned char *m, size_t mlen) {

  size_t ol;
  unsigned char *cwcenc;
  unsigned char *b64enc;
  int i;

  cwcenc=spc_cipherq_encrypt(q,m,mlen,&ol);
  if(!cwcenc) return NULL;
  b64enc=spc_base64_encode(cwcenc,ol,0);
  if(!b64enc) return NULL;

# ifdef DEBUG_CWCCRYPT
  printf("DEBUG cwcenc (length=%d): ",ol);
  for(i=0;i<ol;i++)
    printf("%d ",cwcenc[i]);
  printf("\n");
  printf("DEBUG base64 encoded: >%s<\n",b64enc);
# endif
  free(cwcenc);
  return b64enc;
}


/**
   @param q      an initialized SPC_CIPHERQ struct
   @param buf    \0 terminated (!!) buffer to decrypt
   @param ol     returned length of the decrypted string (\0 terminated)

   @return       the decrypted string
*/
unsigned char *spc_cipherq_b64decrypt(SPC_CIPHERQ *q, unsigned char *buf,
				      size_t *ol) {

  int err;
  size_t len;
  unsigned char *b64dec;
  unsigned char *cwcdec;

# ifdef DEBUG_CWCCRYPT
  spc_print_cipherq(q);
# endif

  b64dec = spc_base64_decode(buf,&len,1,&err);
  if(!b64dec) return NULL;
  cwcdec=spc_cipherq_decrypt(q,b64dec,len,ol);
  free(b64dec);
  if(!cwcdec) return NULL;
  return cwcdec;
}
