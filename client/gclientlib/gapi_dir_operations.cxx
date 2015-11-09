#include "gapi_dir_operations.h"
#include <string.h>
#include <string>
#include <sstream>
#include <errno.h>
#include <stdlib.h>
#include "gapiUI.h"

#define DEBUG_TAG "GAPI_DIR_OPERATIONS"
#include "debugmacros.h"

#include "posix_helpers.h"
using namespace PosixHelpers;

/** Gapi version of the opendir function call. Called first if you want to scan
    a directory @name. If the operation is successful a GAPI_DIR
    pointer is returned which can be passed on to gapi_readir and
    finally gapi_closedir. If an error occured, the return value is
    NULL and the following errorcodes are put into errno:
    - EACCES: Authentication failed
    - ECONNREFUSED: You need to (re)connect to the gapi server
    - EAGAIN: The server is temporarily unavailable
*/
GAPI_DIR *gapi_opendir(const char *file)
{
  std::string command("ls -la ");
  std::string dirstream("");
  std::string name(file);
  fflush(stdout);
  GAPI_DIR *dir = new GAPI_DIR();
  if(!dir){
    errno=ENOMEM;
    return 0;
  }

  if(urlIsLocal(name)){
    dir->type=sys;
    dir->dir.sys_dir=opendir(name.c_str());
    return dir;
  }

  urlRemotePart(name);

  dir->type=gapi;

  bool nodelete=false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }
  GapiUI *gc = GapiUI::MakeGapiUI();

  command.append(name);

  DEBUGMSG(3, << "Sending command: >" << command << "<" << std::endl);
  
  if(executeRemoteCommand(gc, command)){
    if (!nodelete)delete gc;
    delete dir;
    return 0;
  }
  
#ifndef __APPLE__
  dir->dir.gapi_dir.dir_entry.d_off=0;
#endif
  int column=0;
  for(column=0; ;column++){
    std::map<std::string, std::string> tags;
    gc->readTags(column, tags);
    if(!tags.size()) break;
    dirstream.append(tags["name"]).append(" ");
    //    if ((tags["name"] == ".") && (std::string(file) == "/"))  {
    //      dirstream.append(".. ");
    //      dirstream.append("d ");
    //    }
  };

  if (column==0) {
    if (!nodelete)delete gc;
    delete dir;
    return 0;
  }

  dir->dir.gapi_dir.dirstream=new char[dirstream.size()+1];
  strcpy(dir->dir.gapi_dir.dirstream, dirstream.c_str());

  DEBUGMSG(3, << "Producing Stream:  >" << dir->dir.gapi_dir.dirstream << "<" << std::endl); 
  std::cout << "Producing Stream:  >" << dir->dir.gapi_dir.dirstream << "<" << std::endl; 

  if (!nodelete)delete gc;
  return dir;
}


/** Gapi version of the readdir function call.
 */
struct dirent *gapi_readdir(GAPI_DIR *dir)
{
  if(dir->type == sys) {
    return readdir(dir->dir.sys_dir);
  }
  struct dirent *dir_entry=&dir->dir.gapi_dir.dir_entry;
#ifndef __APPLE__
  int len = strspn(dir->dir.gapi_dir.dirstream+dir_entry->d_off, "\n ");
  dir_entry->d_off+=len;
  len = strcspn(dir->dir.gapi_dir.dirstream+dir_entry->d_off, "\n ");
#else
  int len = strspn(dir->dir.gapi_dir.dirstream, "\n ");
#endif
  if(!len) 
    return 0;
  if(len>255) len=255;
#ifndef __APPLE__
  strncpy(dir->dir.gapi_dir.dir_entry.d_name,
	  dir->dir.gapi_dir.dirstream +dir_entry->d_off, len);
  dir->dir.gapi_dir.dir_entry.d_name[len]='\0';
  dir_entry->d_off += len;
#else
  strncpy(dir->dir.gapi_dir.dir_entry.d_name,
	  dir->dir.gapi_dir.dirstream, len);
  dir->dir.gapi_dir.dir_entry.d_name[len]='\0';
  //  dir_entry->d_off += len;
#endif
  return dir_entry;
}


/** Gapi version of the closedir function call.
 */
int gapi_closedir(GAPI_DIR *dir){
  if(dir->type == sys)
    closedir(dir->dir.sys_dir);
  else
    delete [] dir->dir.gapi_dir.dirstream;

  delete dir;
  return 0;
}


/** Gapi version of the telldir function call.
 */
off_t gapi_telldir(GAPI_DIR *dir)
{
  if(!dir)
    return EBADF;
  
  if(dir->type == sys)
    return telldir(dir->dir.sys_dir);
#ifndef __APPLE__
  return dir->dir.gapi_dir.dir_entry.d_off;
#else
  return 0;
#endif
}


/** Gapi version of the rewinddir function call.
 */
void gapi_rewinddir(GAPI_DIR *dir)
{  
  if(dir->type == sys)
    rewinddir(dir->dir.sys_dir);

#ifndef __APPLE__
  dir->dir.gapi_dir.dir_entry.d_off = 0;
#endif
}


/** Gapi version of the seekdir function call.
 */
void gapi_seekdir(GAPI_DIR *dir, off_t offset)
{
  if(dir->type == sys)
    seekdir(dir->dir.sys_dir, offset);

#ifndef __APPLE__
  dir->dir.gapi_dir.dir_entry.d_off = offset;
#endif
}


/** Gapi version of the getcwd function call. You have to supply a
    buffer @buf and its size @size in which the directory name will be
    coppied. On success, @buf is returned. If you did not provide
    enough space, NULL is returned and errno is set to ERANGE.
*/
char *gapi_getcwd(char *buf, size_t size)
{
  bool nodelete=false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }
  GapiUI *gc = GapiUI::MakeGapiUI();
  
  if(!gc){
    errno=ENOMEM;
    return 0;
  }
  
  if(size-1 < gc->cwd().size()){
    if (!nodelete)delete gc;
    errno = ERANGE;
    return NULL;
  } 

  strncpy(buf, gc->cwd().c_str(), size);

  return buf;
}

/** Gapi version of the chdir function call.
    The return value is always 0!!
*/    
int gapi_chdir(const char *path){
  bool nodelete=false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }
  GapiUI *gc = GapiUI::MakeGapiUI();
  
  if(!gc){
    errno=ENOMEM;
    return 0;
  }

  gc->setCWD(path);
  if (!nodelete) delete gc;
  return 0;
}

/** Gapi version of the mkdir function call. You have to supply a
    buffer @buf and its size @size in which the directory name will be
    coppied. On success, @buf is returned. If you did not provide
    enough space, NULL is returned and errno is set to ERANGE.
    - EACCES: Authentication failed
    - ECONNREFUSED: You need to (re)connect to the gapi server
    - EAGAIN: The server is temporarily unavailable
*/
int gapi_mkdir(const char *pathname, mode_t mode)
{
  // Analyze the pathnmame
  if(urlIsLocal(pathname))
    return mkdir(pathname, mode);

  std::string name=pathname;

  urlRemotePart(name);

  std::stringstream command1;
  std::stringstream command2;
  command1 << "mkdir ";

  bool nodelete=false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }
  GapiUI *gc = GapiUI::MakeGapiUI();
  
  command1 << name;

  DEBUGMSG(3, << "Sending command: >" << command1 << "<" << std::endl);

  if(executeRemoteCommand(gc, command1.str())){
    if (!nodelete)delete gc;
    return -1;
  }

  if(checkResult(gc)) {
    if (!nodelete)delete gc;
    return -1;
  }
 
  command2 << "chmod ";
  // TODO: this does not work properly!
  command2 << std::ios::oct << mode;
  command2 << " " << name;

  DEBUGMSG(3, << "Sending command: >" << command2 << "<" << std::endl);

  if(executeRemoteCommand(gc, command2.str())){
    if (!nodelete)delete gc;
    return -1;
  }

  if(checkResult(gc)) {
    if (!nodelete)delete gc;
    return -1;
  }

  if (!nodelete)delete gc;
  return 0;

}


/** Gapi version of the rmdir function call. You have to supply a
    LFN in @pathname.
    On success 0 is returned otherwise -1 and the following errorcodes
    are set in errno:
    - EACCES: Authentication failed
    - ECONNREFUSED: You need to (re)connect to the gapi server
    - EAGAIN: The server is temporarily unavailable    
*/
int gapi_rmdir(const char *pathname)
{
  // Analyze the pathnmame
  if(urlIsLocal(pathname))
    return rmdir(pathname);

  std::string name=pathname;

  urlRemotePart(name);

  std::string command="rmdir ";
  command.append(name);

  bool nodelete=false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }
  GapiUI *gc = GapiUI::MakeGapiUI();

  DEBUGMSG(3, << "Sending command: >" << command << "<" << std::endl);
  
  if(executeRemoteCommand(gc, command)){
    if(!nodelete)delete gc;
    return -1;
  }

  if(checkResult(gc)) {
    if (!nodelete)delete gc;
    return -1;
  }

  if (!nodelete)delete gc;
  return 0;
}
