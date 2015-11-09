#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>


volatile void *spc_memset(volatile void *dst, int c, size_t len) {
  volatile char *buf;

  for (buf = (volatile char *)dst;  len;  buf[--len] = c);
  return dst;
}


void spc_make_fd_nonblocking(int fd) {
  int flags;

  flags = fcntl(fd, F_GETFL);  /* Get flags associated with the descriptor. */
  if (flags == -1) {
    perror("spc_make_fd_nonblocking failed on F_GETFL");
    exit(-1);
  }
  flags |= O_NONBLOCK;
  /* Now the flags will be the same as before, except with O_NONBLOCK set.
   */
  if (fcntl(fd, F_SETFL, flags) == -1) {
    perror("spc_make_fd_nonblocking failed on F_SETFL");
    exit(-1);
  }
}






static int spc_devrand_fd           = -1,
           spc_devrand_fd_noblock =   -1,
           spc_devurand_fd          = -1;

void spc_rand_init(void) {
  spc_devrand_fd         = open("/dev/random",  O_RDONLY);
  spc_devrand_fd_noblock = open("/dev/random",  O_RDONLY);
  spc_devurand_fd        = open("/dev/urandom", O_RDONLY);

  if (spc_devrand_fd == -1 || spc_devrand_fd_noblock == -1) {
    perror("spc_rand_init failed to open /dev/random");
    exit(-1);
  }
  if (spc_devurand_fd == -1) {
    perror("spc_rand_init failed to open /dev/urandom");
    exit(-1);
  }
  spc_make_fd_nonblocking(spc_devrand_fd_noblock);
}

unsigned char *spc_rand(unsigned char *buf, size_t nbytes) {
  ssize_t       r;
  unsigned char *where = buf;

  if (spc_devrand_fd == -1 && spc_devrand_fd_noblock == -1 && spc_devurand_fd == -1)
    spc_rand_init();
  while (nbytes) {
    if ((r = read(spc_devurand_fd, where, nbytes)) == -1) {
      if (errno == EINTR) continue;
      perror("spc_rand could not read from /dev/urandom");
      exit(-1);
    }
    where  += r;
    nbytes -= r;
  }
  return buf;
}

unsigned char *spc_keygen(unsigned char *buf, size_t nbytes) {
  ssize_t       r;
  unsigned char *where = buf;

  if (spc_devrand_fd == -1 && spc_devrand_fd_noblock == -1 && spc_devurand_fd == -1)
    spc_rand_init();
  while (nbytes) {
    if ((r = read(spc_devrand_fd_noblock, where, nbytes)) == -1) {
      if (errno == EINTR) continue;
      if (errno == EAGAIN) break;
      perror("spc_rand could not read from /dev/random");
      exit(-1);
    }
    where  += r;
    nbytes -= r;
  }
  spc_rand(where, nbytes);
  return buf;
}

unsigned char *spc_entropy(unsigned char *buf, size_t nbytes) {
  ssize_t       r;
  unsigned char *where = buf;

  if (spc_devrand_fd == -1 && spc_devrand_fd_noblock == -1 && spc_devurand_fd == -1)
    spc_rand_init();
  while (nbytes) {
    if ((r = read(spc_devrand_fd, (void *)where, nbytes)) == -1) {
      if (errno == EINTR) continue;
      perror("spc_rand could not read from /dev/random");
      exit(-1);
    }
    where  += r;
    nbytes -= r;
  }
  return buf;
}
