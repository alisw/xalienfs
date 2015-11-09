#ifndef _spc_cwc_h
#define _spc_cwc_h

#ifdef __cplusplus
extern "C" {
#endif

#include <limits.h>

#if defined(UINT_MAX) && UINT_MAX == 4294967295
  typedef   unsigned int  uint32;
#else
#  error mode(32t) must be an unsigned 32-bit type (see the header file)
#endif

#if defined(ULONG_LONG_MAX) && ULONG_LONG_MAX == 18446744073709551615ull
  typedef unsigned long long uint64;
#else
#  error mode(64t) must be an unsigned 64-bit type (see the header file)
#endif

#ifndef CONFIG_H_INCLUDED
#define CONFIG_H_INCLUDED
#include "config.h"
#endif

#ifdef USE_OPENSSL_AES
/*  Here follow the definitions for using the OpenSSL AES implementation */
/*  (requires OpenSSL version >= 0.9.7)   */
#  include <openssl/aes.h>

/* The type alias AES_KS_T must be set to the type of the AES key
 * schedule you're using.
 */
typedef AES_KEY AES_KS_T;

/* This macro must implement the basic AES key expansion operation
 * for ECB encryption.  The parameters are a key, the bit length of
 * the key and a pointer to a key schedule to use for output.
 */
#  define CWC_AES_SETUP(key, bitlen, ks)  \
                AES_set_encrypt_key(key, bitlen, ks)

/* This macro must implement the basic AES block encryption
 * operation. It takes a pointer to a key schedule, a pointer to an
 * input block and a pointer to the output block.
 */
#  define CWC_AES_ENCRYPT(ks, in, out) AES_encrypt(in, out, ks)

#else
/*  Definitions for Brian Gladman's implementation of AES  */
#  include "gaes/aes.h"
typedef aes_encrypt_ctx AES_KS_T;
#  define CWC_AES_SETUP(key, bitlen, ks)  \
                aes_encrypt_key(key, bitlen, ks)
#  define CWC_AES_ENCRYPT(ks, in, out) aes_encrypt(in, out, ks)
#endif

typedef unsigned char      uchar;

typedef struct {
  uint32       hashkey[4];
  AES_KS_T     aeskey;
} cwc_t;

/* The public API. */
/* Warning: cwc_init zeros out the key before exiting! */
int cwc_init(cwc_t ctx[1], uchar key[], int keybits);
void cwc_encrypt(cwc_t ctx[1], uchar a[], uint32 alen, uchar pt[],
		 uint32 ptlen, uchar nonce[11], uchar output[]);
int cwc_decrypt(cwc_t ctx[1], uchar a[], uint32 alen, uchar ct[],
		uint32 ctlen, uchar nonce[11], uchar output[]);
void cwc_cleanup(cwc_t ctx[1]);

#ifdef __cplusplus
}
#endif
#endif
