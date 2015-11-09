#ifndef GAPI_FILE_OPERATIONS_H
#define GAPI_FILE_OPERATIONS_H

#include <sys/types.h>
#include <fcntl.h>

#define O_PUBLIC 0x8000

#ifdef __cplusplus
extern "C" {
#endif
struct gapi_file_info {
  int mode;
  int flags;
  char lfn[4096];
  char envelope[16384];
};

int gapi_open(const char *pathname, int flags, ...);

int gapi_creat(const char *pathname, mode_t mode);

ssize_t gapi_read  (int fd, void *buf, size_t count);
ssize_t gapi_pread (int fd, void *buf, size_t count, off_t offset);
ssize_t gapi_write (int fd, const void *buf, size_t count);
ssize_t gapi_pwrite(int fd, const void *buf, size_t count, off_t offset);

int gapi_close(int fd);

off_t gapi_lseek(int fildes, off_t offset, int whence);

int gapi_unlink(const char *pathname);

int gapi_fstat(int filedes, struct stat *buf);

int gapi_copy(int arg, char *argv[], long buffersize); 

long long gapi_size(int fd) ;

int gapi_chmod(const char *path, mode_t mode);

int gapi_rename(const char *oldpath, const char *newpath);

int gapi_find(const char *path, const char *pattern, int maxresult);

const char* gapi_getfindresult(const char* tag, int row); 

int gapi_isonline(const char* path);

int gapi_prepare(const char* path, int flag=8);

int gapi_accessurl(const char* path , int flags, char* rooturl, char* envelope, char* lfn, int maxlen);

#ifdef __cplusplus
}
#endif

#endif  // #ifndef GAPI_FILE_OPERATIONS_H
