#ifndef GAPI_STAT_H
#define GAPI_STAT_H

// #define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

const int gapi_stat_cachetime=30; // 30 seconds

typedef struct stat GAPI_STAT;

struct GAPI_STAT_NODE {
  char pathname[4096];
  GAPI_STAT* st;
  time_t expires;
};


#ifdef __cplusplus
extern "C" {
#endif

  int gapi_stat(const char *file_name, GAPI_STAT *buf);

  int gapi_lstat(const char *file_name, GAPI_STAT *buf);

  int gapi_access(const char *filepath, int mode);

  void  gapi_perror(const char *msg);

  char *gapi_serror();
#ifdef __cplusplus
}
#endif

#endif  // #ifndef GAPI_STAT_H
