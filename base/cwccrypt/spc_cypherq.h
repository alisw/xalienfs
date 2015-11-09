#ifndef _spc_cypherq_h
#define _spc_cypherq_h

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "spc_cwc.h"

#define MAX_KEY_LEN (32)  /* 256 bits */

#define SPC_BLOCK_SZ 16

#define SPC_DEFAULT_KEYSIZE 16
#define SPC_DEFAULT_NONCESIZE 16 /* TODO: Replace SHMAUTH_NONCE_SIZE */

typedef struct {
  cwc_t         ctx;
  unsigned char nonce[SPC_BLOCK_SZ];
} SPC_CIPHERQ;

void spc_print_cipherq(SPC_CIPHERQ* q);
void spc_cipherq_setnonce(SPC_CIPHERQ* q, unsigned char *nonce);
  unsigned char *spc_cipherq_getnonce(SPC_CIPHERQ* q);
size_t spc_cipherq_setup(SPC_CIPHERQ *q, unsigned char *basekey, size_t keylen,
                         size_t keyuses);
void spc_cipherq_cleanup(SPC_CIPHERQ *q);
  //static void increment_counter(SPC_CIPHERQ *q);
unsigned char *spc_cipherq_encrypt(SPC_CIPHERQ *q, unsigned char *m, size_t mlen,
                                   size_t *ol);
unsigned char *spc_cipherq_decrypt(SPC_CIPHERQ *q, unsigned char *m, size_t mlen,
                                   size_t *ol);
unsigned char *spc_cipherq_b64encrypt(SPC_CIPHERQ *q, unsigned char *m, size_t mlen);
unsigned char *spc_cipherq_b64decrypt(SPC_CIPHERQ *q, unsigned char *buf,
				      size_t *ol);
#ifdef __cplusplus
}
#endif
#endif
