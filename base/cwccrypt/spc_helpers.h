#ifndef _spc_helpers_h
#define _spc_helpers_h

#ifdef __cplusplus
extern "C" {
#endif

volatile void *spc_memset(volatile void *dst, int c, size_t len);
void spc_make_fd_nonblocking(int fd);
void spc_rand_init(void);
unsigned char *spc_rand(unsigned char *buf, size_t nbytes);
unsigned char *spc_keygen(unsigned char *buf, size_t nbytes);
unsigned char *spc_entropy(unsigned char *buf, size_t nbytes);

#ifdef __cplusplus
}
#endif

#endif
