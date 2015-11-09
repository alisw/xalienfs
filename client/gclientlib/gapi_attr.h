#ifndef GAPI_ATTR_H
#define GAPI_ATTR_H

#include <sys/types.h>
#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

ssize_t gapi_getxattr (const char *path, const char *name,
			void *value, size_t size);

ssize_t gapi_lgetxattr (const char *path, const char *name,
			 void *value, size_t size);

int gapi_setxattr (const char *path, const char *name,
		    const void *value, size_t size, int flags);

int gapi_lsetxattr (const char *path, const char *name,
		     const void *value, size_t size, int flags);

ssize_t gapi_listxattr (const char *path,
			 char *list, size_t size);

ssize_t gapi_llistxattr (const char *path,
		    char *list, size_t size);

int gapi_removexattr (const char *path, const char *name);
int gapi_lremovexattr (const char *path, const char *name);



#ifdef __cplusplus
}
#endif

#endif  // #ifndef GAPI_ATTR_H
