#ifndef _spc_b64_h
#define _spc_b64_h

#ifdef __cplusplus
extern "C" {
#endif

/* Base64 encoding/decoding */
unsigned char *spc_base64_encode(unsigned char *input, size_t len, int wrap);
unsigned char *spc_base64_decode(unsigned char *buf, size_t *len, int strict,
                                 int *err);
#ifdef __cplusplus
}
#endif

#endif
