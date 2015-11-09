#ifndef POSIX_HELPERS_H
#define POSIX_HELPERS_H

#include "config.h"
#include <string>
#include <vector>

#include "gapiUI.h"

#include <semaphore.h>

#define GAPI_FD_OFFSET 65535*2

extern int gapi_errno;

namespace PosixHelpers{

  class FileDescriptorManager {
  public:
    FileDescriptorManager();
    ~FileDescriptorManager();
    bool isValid(int fd) const;
    void *operator[](int i) const;
    void setFD (int i, void *client);
    int getNewFD(int i);
    void releaseFD(int fd);
    int checkFD(int fd) const;
  private:
    std::vector<void *> fileDescriptors;
    mutable sem_t sem;
  };

  extern class FileDescriptorManager fileDescriptors;

  int checkFD(int fd);


  bool pfnFromCatalogue(const std::string &lfn,
			std::string &protocol, std::string &host,
			int &port, std::string &se, std::string &path, int accessmode);

  bool urlIsLocal(const std::string &url);

  void urlRemotePart(std::string &url);

  int checkResult(GapiUI *gc);

  int executeRemoteCommand(GapiUI *gc, const std::string &command);

  void findandreplace( std::string & source, const std::string & find, const std::string & replace);
}

#endif
