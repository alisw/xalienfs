#ifndef GAPI_DIR_OPERATIONS_H
#define GAPI_DIR_OPERATIONS_H

#include <sys/types.h>
#include <dirent.h>

struct GAPI_DIR_STRUCT {
  char *dirstream;
  struct dirent dir_entry;
};


enum dir_type {gapi, sys};
typedef struct {
  enum dir_type type;
  union {
    struct GAPI_DIR_STRUCT gapi_dir;
    DIR *sys_dir;
  } dir;
} GAPI_DIR;


#ifdef __cplusplus
extern "C" {
#endif

  GAPI_DIR *gapi_opendir(const char *name);

  struct dirent *gapi_readdir(GAPI_DIR *dir);
  
  int gapi_closedir(GAPI_DIR *dir);
  
  off_t gapi_telldir(GAPI_DIR *dir);
  
  void gapi_rewinddir(GAPI_DIR *dir);
  
  void gapi_seekdir(GAPI_DIR *dir, off_t offset);
  
  int gapi_mkdir(const char *pathname, mode_t mode);
  
  int gapi_rmdir(const char *pathname);

  char *gapi_getcwd(char *buf, size_t size);

  int gapi_chdir(const char *path);

#ifdef __cplusplus
}
#endif

#endif  // #ifndef GAPI_DIR_OPERATIONS_H
