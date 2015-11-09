#include "posix_helpers.h"
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdlib.h>
#include <string.h>

int gapi_errno=0;

namespace PosixHelpers{
class FileDescriptorManager fileDescriptors;

FileDescriptorManager::FileDescriptorManager()
{
  sem_init (&sem, 0, 1);
}


FileDescriptorManager::~FileDescriptorManager()
{
  sem_destroy(&sem);
}


void *FileDescriptorManager::operator[] (int i) const
{
  sem_wait(&sem);
  void *client=fileDescriptors[i];
  sem_post(&sem);
  return client;
}


void FileDescriptorManager::setFD (int i, void *client)
{
  sem_wait(&sem);
  fileDescriptors[i]=client;
  sem_post(&sem);
}


int FileDescriptorManager::getNewFD(int fd)
{
  //  printf("fd is %d [size %d]\n",fd,(int)fileDescriptors.size());
  sem_wait(&sem);
  if ((int)fileDescriptors.size() <= fd) {
    fileDescriptors.resize(fd*2+1);
  }
  fileDescriptors[fd]=(void*)1;
  sem_post(&sem);
  return fd;
}


void FileDescriptorManager::releaseFD(int fd)
{
  sem_wait(&sem);
  //  delete fileDescriptors[fd];
  fileDescriptors[fd]=NULL;
  sem_post(&sem);
}


/** Helper functions which subtracts the offset on gapi file file
    descriptor numbers and checks whether this offset is valid
*/
int FileDescriptorManager::checkFD(int fd) const
{
  sem_wait(&sem);
  //  fd -= GAPI_FD_OFFSET;
  
  if(fd<0 || fd > (int)fileDescriptors.size()){
    sem_post(&sem);
    errno = EBADF;
    return -1;
  }

  sem_post(&sem);
  return 0;
}

bool urlIsLocal(const std::string &url)
{
  if(url.find("/alien/",0) == 0)
    return false;
  if(url.find("alien:",0) == 0)
    return false;
  if (url.find("file:",0) == 0)
    return true;
  if (getenv("alien_MODE")) {
    if (!(strcmp(getenv("alien_MODE"),"GRID"))) {
      return false;
    }
  }
  //  std::cout << url << " is local. " << std::endl;
  return true;
}


void urlRemotePart(std::string &url)
{
  int sep;
  int sep2;
  if((sep=url.find("/alien/",0)) == 0)
    url=url.substr(6);
  if((sep=url.find("alien://",0)) == 0)
    url=url.substr(8);
  if((sep=url.find("alien:",0)) == 0)
    url=url.substr(6);
  if((sep=url.find("file://",0)) == 0)
    url=url.substr(7);
  if((sep=url.find("file:",0)) == 0)
    url=url.substr(5);
  if((sep=url.find("root://",0)) == 0) {
    sep2=url.find("/",7);
    url=url.substr(sep2+1);
  }

  //  std::cout << "Remote Part is " << url << std::endl;
}

/** Helper functions which subtracts the offset on gapi file file
    descriptor numbers and checks whether this offset is valid
    returns -1, if no result was deliverd,
    returns 0, if result 1 was delivered,
    returns 1, if result 0 was delivered.
*/

int checkResult(GapiUI *gc) {
  if (!gc) 
    return -1;

  std::map<std::string, std::string> tags;
  gc->readTags(0, tags);
  int result=0;
  if (tags["__result__"].length()){
    if ((result = atoi(tags["__result__"].c_str()))) {
      return !result;
    }
  }
  return -1;
}
    
int executeRemoteCommand(GapiUI *gc, const std::string &command)
{
  if(!gc){
    errno=ENOMEM;
    return -1;
  }

  if(!gc->Connected()) {
    errno=ECONNREFUSED;
    return -1;
  }

  if(!gc->Command(command.c_str())) {
    t_gerror gerror = gc->GetErrorType();
    if(gc->ErrorMatches(gerror,"server:authentication")) {
      errno=EACCES;
      return -1;
    }
    if(gc->ErrorMatches(gerror,"server:session_expired")) {
      //DEBUGMSG(1,<< "session expired, reconnecting...\n");
      if(!gc->Reconnect()) {
	//DEBUGMSG(1,<< "unable to reconnect to " << gc->GetAuthenURL() <<"\n");
	errno=EACCES;
	return -1;
      }
      gc->Command(command.c_str());
      return 0;
    }
    std::cerr << gerror << std::endl;
    errno = EAGAIN;
    return -1;
  }
  return 0;
}

void findandreplace(std::string & source, const std::string & find, const std::string & replace) {
  size_t j=0;
  for (;(j = source.find( find , j) ) != std::string::npos;) {
    source.replace(j, find.length(), replace );
    j+= replace.length();
  }
}




} // namespace PosixHelpers
