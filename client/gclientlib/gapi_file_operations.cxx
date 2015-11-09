#include "gapi_file_operations.h"

#include "config.h"
#include <string>
#include <vector>
#include <iostream>
#include <sstream>

#include <errno.h>
#include <assert.h>
#include <stdarg.h>

#include "posix_helpers.h"
#include "../CODEC/CODEC.h"
#include <errno.h>
#include <sys/time.h>

#ifdef XROOTD
#include "XrdPosix/XrdPosixXrootd.hh"
#include "XrdClient/XrdClientAdmin.hh"
#include "XrdClient/XrdClientDebug.hh"
#endif

#define DEBUG_TAG "GAPI_FILE_OPERATIONS"
#include "debugmacros.h"

using namespace PosixHelpers;

extern "C" {
   /////////////////////////////////////////////////////////////////////
// function + macro to allow formatted print via cout,cerr
/////////////////////////////////////////////////////////////////////
 void cout_print(const char *format, ...)
 {
    char cout_buff[4096];
    va_list args;
    va_start(args, format);
    vsprintf(cout_buff, format,  args);
    va_end(args);
    std::cout << cout_buff;
 }

   void cerr_print(const char *format, ...)
   {
      char cerr_buff[4096];
      va_list args;
      va_start(args, format);
      vsprintf(cerr_buff, format,  args);
      va_end(args);
      std::cerr << cerr_buff;
   }

#define COUT(s) do {                            \
      cout_print s;                             \
   } while (0)

#define CERR(s) do {                            \
      cerr_print s;                             \
   } while (0)

}


/** Gapi version of the open function call. This var-args function
    can be called in two versions (as the UNIX open call):
    int open(const char *path, int flags);
    int open(const char *path, int flags, mode_t mode); 
    @pathname is a URL of the form hostname:port/file?transferflags,
    a path /alien/... or a local path in which case the local open()
    call is used.
    @flags are the normal posix flags for opening a file and mode is
    the mode (access permissions) with which the file is created.

    On success the gapi file descriptor of the file is returned
    which is an int >=0. If an error occours, the errornumber is returned
    and errno is set accordingly. The follwoing errors are used:
    - ENOENT: The URL is malformed or the file does not exist
    - EACCES: Access denied at server
    - ENOMEM: Out of memory
    - EAGAIN: Too many connections, try again

    Description of the URL:
    If root:// is found at the beginning, we talk to the SE,
    if we find /alien/ or alien:// then we ask the file catalogue for help,
    otherwise we assume that we open a local file and forward the
    call to the open system call

    Files can be either written or read
*/
int gapi_open(const char *pathname, int flags, ...)
{
  std::string url=pathname;
  mode_t mode;
  int fd = -1;

  // test
  if( (flags & O_CREAT)==O_CREAT ){
    va_list ap;
    va_start(ap, flags);
    mode=(mode_t) va_arg(ap, int);
    va_end(ap);
    if(urlIsLocal(url)){
      int openreturn = open(url.c_str(), flags, mode);
      return openreturn;
    }
  }

  if(urlIsLocal(url)){
    return open(url.c_str(), flags);
  }

  char rooturl[4096];
  char lfn[4096];
  char envelope[4096];

  int maxlen = 4096;
  
  if (gapi_accessurl(pathname, flags,rooturl,envelope,lfn,maxlen)) {
    errno = EACCES;
    return -1;
  }
      
  struct gapi_file_info* ginfo = new struct gapi_file_info;
  ginfo->mode = mode;
  ginfo->flags = flags;
  strcpy(ginfo->envelope,envelope);
  strcpy(ginfo->lfn,lfn);

  // open the file
#ifdef XROOTD
  if( (flags & O_CREAT) && O_CREAT ){
    fd = XrdPosixXrootd::Open(rooturl,flags,mode);
  } else {
    fd = XrdPosixXrootd::Open(rooturl,flags);
  }

  // expand the filedescriptor array
  fileDescriptors.getNewFD(fd-XrdPosixFD);
  // store the access envelope in the filedescriptor array
  fileDescriptors.setFD(fd-XrdPosixFD, (void*)ginfo);
#endif
  return fd;
}

/** Gapi access function to translate alien filenames to TURLs
    stores the rooturl, envelope and lfn in the arguements.
    The strings have to provide enough allocated memeory at the pointer positions
    maxlen indicates the size of the 3 char* rooturl, envelope & lfn
    - returns 0 if file can be access, otherwise -1
*/
int gapi_accessurl(const char* pathname, int flags, char* retrooturl, char* retenvelope, char* retlfn, int maxlen)
{
  std::string url=pathname;
  std::string rooturl = "";
  std::string envelope = "";
  bool readmode=false;
  bool publicaccess=false;

  urlRemotePart(url);
  // get the access envelope
  std::string command;
  DEBUGMSG(3, << "Accessing URL: >" << pathname << "<" << "Flags: >" << flags << "<" << std::endl);

  readmode=true;
  
  if (flags & O_CREAT) {
    command.append("access write-once ");
    readmode=false;
  } else {
    if (flags & O_PUBLIC) {
      publicaccess=true;
    } else {
      if (flags & O_WRONLY) {
	command.append("access write-version ");
	readmode=false;
      }
      
      if (flags & O_RDWR) {
	command.append("access write-version ");
	readmode=false;
      }
    }
  }

  if (readmode) {
    if (publicaccess) {
      command.append("access -p read ");
    } else {
      command.append("access read ");
    }
  }

  if (command.length() ==0) {
    errno=EIO;
    return -1;
  }


  // split the pathname-url into lfn, options
  std::string lfn;
  std::string options;
  int pos = url.find ("?",0);
  std::map <std::string,std::string> optionmap;

  if (pos == (int)std::string::npos) {
    // no options
    lfn = url;
  } else {
    lfn = url.substr(0,pos);
    options = url.substr(pos+1, url.length()-pos-1);
    // split the options by &
    pos = 0;
    int lastpos=-1;
    bool stopit=false;
    do { 
      pos = options.find ("&",0);

      if (pos == (int)std::string::npos) {
	stopit = true;
	pos = options.length();
      }

      std::string suboption = options.substr(lastpos+1,pos-lastpos-1);
      int equalpos = suboption.find("=",0);
      if (equalpos == (int)std::string::npos) 
	break;
      std::string suboptiontag = suboption.substr(0,equalpos);
      std::string suboptionvalue = suboption.substr(equalpos+1, suboption.length()-equalpos-1);
      optionmap[suboptiontag] = suboptionvalue;
      lastpos=pos;
    } while (!stopit);

  }

  command.append(lfn);
  std::string storageelement="";

  // get the se option or the default storageelement
  if (optionmap["se"].length()>0) {
    storageelement = optionmap["se"];
  } else {
    if (readmode && getenv("alien_CLOSE_SE")) {
      storageelement = getenv("alien_CLOSE_SE");
    }
  }

  // append the se if specified
  if (storageelement.length()) {
    command.append(" ");
    command.append(storageelement);
  }

  bool nodelete=false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }
  GapiUI *gc = GapiUI::MakeGapiUI();
  
  DEBUGMSG(3, << "Sending command: >" << command << "<" << std::endl);
  
  if(executeRemoteCommand(gc, command)){
    if (!nodelete)delete gc;
    errno=EIO;
    return -1;
  }
  
  // check the result
  std::map<std::string, std::string> tags;
  int column=0;
  int row =0;
  bool success;
  success=false;
  while ((row = gc->readTags(column, tags))) {
    // got one row
    if ( tags["url"].length()>0) {
      rooturl = tags["url"];
      success = true;
    } else {
      success = false;
    }

    if ( tags["envelope"].length() >0) {
      envelope = tags["envelope"];
      success = true;
    } else {
      success = false;
    }

    break;
  }
  
  // unlock the UI
  if (!nodelete)delete gc;

  if (!success) {
    DEBUGMSG(3, << "Could not get the URL for >" << pathname <<"<" << std::endl);
    return -2;
  }

  DEBUGMSG(3, << "Accessing RootURL: >" << rooturl << "<" << std::endl);
  DEBUGMSG(3, << "Accessing with envelope: >" << envelope << "<" << std::endl);

  // append the authorization information
  rooturl += std::string("?&authz=");
  rooturl += envelope;

  if ( (int)rooturl.length() < maxlen) {
    sprintf(retrooturl,"%s",rooturl.c_str());
    if ((int)envelope.length() < maxlen) {
      sprintf(retenvelope,"%s",envelope.c_str());
      if ((int)lfn.length() < maxlen) {
	sprintf (retlfn,"%s",lfn.c_str());
	return 0;
      }
    }
  }
  return -1;
}

/** checks if a file is online
    - returns 0 if file is staged or on local disk
 */

int gapi_isonline(const char* path)
{
  //  we now assume also the alien:// prefix
  //  if(urlIsLocal(path)){
  //    return 0;
  //  }

  char rooturl[4096];
  char lfn[4096];
  char envelope[4096];
  int maxlen = 4096;
  int result=0;
  rooturl[0] = 0;

  if (strncmp(path,"root:",5)) {
    if ( (result = gapi_accessurl(path, O_PUBLIC ,rooturl,envelope,lfn,maxlen))) {
      
      if (result == -1) {
	errno = EACCES;
	return -2;
      }
      if (result == -2) {
	errno = ENOENT;
	return -3;
      }
    }
  } else {
    sprintf(rooturl,"%s",path);
  }
  
#ifdef XROOTD
  // fix for archive files
  char* anchor=NULL;
  if ( (anchor = strchr(rooturl,'#')) ) {
    *anchor = 0;
  }

  std::string urlstring=rooturl;
  urlRemotePart(urlstring);

  vecBool vb;
  vecString vs;
  XrdOucString pathname = urlstring.c_str();
  vs.Push_back(pathname);

  // check for DPM
  if (!strncmp(urlstring.c_str(),"/dpm/",5)) {
    // dpm is always online
    return 0;
  }
  // check for dCache
  if (!strncmp(urlstring.c_str(),"/pnfs/",6)) {
    // dCache has not the protocol implemented - assume online
    return 0;
  }

  DebugSetLevel(-1);
  XrdClientAdmin *genadmin = 0;
  genadmin = new XrdClientAdmin(rooturl);
  DebugSetLevel(-1);
  // Then connect
  if (!genadmin->Connect()) {
    delete genadmin;
    genadmin = 0;
    return -1;
  }

  DEBUGMSG(3, << "Accessing URL: >" << pathname.c_str() << "<" << std::endl);
  std::cout << "Accessing URL: >" << pathname.c_str() << "<" << std::endl;
  genadmin->IsFileOnline(vs,vb);
  if (!genadmin->LastServerResp()) {
    fprintf(stderr,"gapi_file_operations::isonline : got no server response\n");
    return -1;
  }

  switch (genadmin->LastServerResp()->status) {
  case kXR_ok:
    delete genadmin;
    if (vb[0] == 1 ) {
      return 0;
    }
    else {
      return -1;
    }
  case kXR_error:
    fprintf(stderr,"gapi_file_operations::isonline : Got %d : %s\n",genadmin->LastServerError(),genadmin->LastServerError()->errmsg);
    break;
  default:
    fprintf(stderr,"gapi_file_operations::isonline : Got %x : %s\n",genadmin->LastServerError(),genadmin->LastServerError()->errmsg);
    break;
  }
  delete genadmin;
#endif
  return -1;
}

int gapi_prepare(const char* path, int flag) 
{
  //  we now also assume the alien:// prefix
  //  if(urlIsLocal(path)){
  //    return 0;
  //  }

  char rooturl[4096];
  char lfn[4096];
  char envelope[4096];
  int maxlen = 4096;
  int result=0;
  rooturl[0] = 0;

  if (strncmp(path,"root:",5)) {
    if ( (result = gapi_accessurl(path, O_PUBLIC ,rooturl,envelope,lfn,maxlen))) {
      
      if (result == -1) {
	errno = EACCES;
	return -2;
      }
      if (result == -2) {
	errno = ENOENT;
	return -3;
      }
    }
  } else {
    sprintf(rooturl,"%s",path);
  }
  
#ifdef XROOTD
  // fix for archive files
  char* anchor=NULL;
  if ( (anchor = strchr(rooturl,'#')) ) {
    *anchor = 0;
  }
  DebugSetLevel(-1);
  XrdClientAdmin *genadmin = 0;
  genadmin = new XrdClientAdmin(rooturl);
  DebugSetLevel(-1);
  // Then connect
  if (!genadmin->Connect()) {
    delete genadmin;
    genadmin = 0;
    return -1;
  }

  std::string urlstring=rooturl;

  urlRemotePart(urlstring);

  vecBool vb;
  vecString vs;
  XrdOucString pathname = urlstring.c_str();
  vs.Push_back(pathname);
  DEBUGMSG(3, << "Preparing URL: >" << pathname.c_str() << "<" << std::endl);
  genadmin->Prepare(vs,flag,0);
  if (!genadmin->LastServerResp()) {
    return -1;
  }

  switch (genadmin->LastServerResp()->status) {
  case kXR_ok:
    delete genadmin;
    return 0;
    case kXR_error:
      //      fprintf(stderr,"gapi_file_operations::isonline : Error %d : %s\n",genadmin->LastServerError(),genadmin->LastServerError()->errmsg);
      break;
    default:
      break;
    }
  delete genadmin;
#endif

  return -1;
}

int gapi_creat(const char *pathname, mode_t mode)
{
  std::cerr << "gapi_creat is not yet implemented" << std::endl;
  abort();
}


/** Gapi version of the read function call. @fd is a gapi file
    descriptor as returned by the gapi_open call. @buf is a buffer
    which is being filled with the read data which has at most @count
    bytes lengt. The return value is the number of bytes read
    (<=@count) in case of success, 0 if the end of the file was
    encountered or -1 in case of error in which case errno will be set
    to an error code. The following error codes are used:
    - EBADF: The @fd file descriptor is bad
    - EAGAIN: Too many connections, try again
    - EIO: IO error, the connection had some problem
*/
ssize_t gapi_read(int fd, void *buf, size_t count)
{
#ifdef XROOTD
  if (fd<XrdPosixFD)
    return read(fd, buf, count);  // System read call
  
  return XrdPosixXrootd::Read(fd,buf,count);
# else
  return 0;
#endif
}

/** Gapi version of the pread function call. @fd is a gapi file
    descriptor as returned by the gapi_open call. @buf is a buffer
    which is being filled with the read data which has at most @count
    bytes lengt. The return value is the number of bytes read
    (<=@count) in case of success, 0 if the end of the file was
    encountered or -1 in case of error in which case errno will be set
    to an error code. The following error codes are used:
    - EBADF: The @fd file descriptor is bad
    - EAGAIN: Too many connections, try again
    - EIO: IO error, the connection had some problem
*/
ssize_t gapi_pread(int fd, void *buf, size_t count,off_t offset)
{
#ifdef XROOTD
  if (fd<XrdPosixFD)
    return pread(fd, buf, count,offset);  // System read call
  
  return XrdPosixXrootd::Pread(fd,buf,count,offset);
# else
  return 0;
#endif
}


/** Gapi version of the write function call. @fd is a gapi file
    descriptor as returned by the gapi_open call. @buf is a buffer
    which contains the data to write with length @count.
    The return value is the number of bytes written
    (<=@count) in case of success, and -1 if there was an error in
    which case errno will be set to an error code. The following error
    codes are used:
    - EBADF: The @fd file descriptor is bad
    - EAGAIN: Too many connections, try again
    - EIO: IO error, the connection had some problem
*/
ssize_t gapi_write(int fd, const void *buf, size_t count)

{
#ifdef XROOTD
  if (fd<XrdPosixFD) {
    return write(fd, buf, count);  // System write call
  }
  
  return XrdPosixXrootd::Write(fd,buf,count);
#else 
  return 0;
#endif
}

/** Gapi version of the pwrite function call. @fd is a gapi file
    descriptor as returned by the gapi_open call. @buf is a buffer
    which contains the data to write with length @count.
    The return value is the number of bytes written
    (<=@count) in case of success, and -1 if there was an error in
    which case errno will be set to an error code. The following error
    codes are used:
    - EBADF: The @fd file descriptor is bad
    - EAGAIN: Too many connections, try again
    - EIO: IO error, the connection had some problem
*/
ssize_t gapi_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
#ifdef XROOTD
  if (fd<XrdPosixFD)
    return pwrite(fd, buf, count, offset);  // System write call
  
  return XrdPosixXrootd::Pwrite(fd,buf,count, offset);
#else 
  return 0;
#endif
}


/** Gapi version of the close function call. @fd is a gapi file
    descriptor as returned by the gapi_open call. Returns -1 in case
    of error and sets errno to the following error codes:
    - EBADF: The @fd file descriptor is bad
*/
int gapi_close(int fd)
{
  int result = -1;
#ifdef XROOTD
  if (fd<XrdPosixFD)
    return close(fd);  // System close call

  struct gapi_file_info* ginfo = (struct gapi_file_info*) fileDescriptors[fd-XrdPosixFD];
  if (ginfo) {
    if ( (ginfo->flags & O_WRONLY) || (ginfo->flags & O_RDWR) || (ginfo->flags & O_CREAT) ) {
      std::string command;
      bool nodelete=false;
      if (GapiUI::GetGapiUI() ) {
	nodelete = true;
      }
      GapiUI *gc = GapiUI::MakeGapiUI();
      command.append("commit");
      // lock the UI
      setenv("GCLIENT_EXTRA_ARG",ginfo->envelope,1);
      DEBUGMSG(3, << "Sending command: >" << command << "<" << std::endl);
    
      if(executeRemoteCommand(gc, command)){
	unsetenv("GCLIENT_EXTRA_ARG");
	if(!nodelete)delete gc;
	errno=EIO;
	return result;
      }
      unsetenv("GCLIENT_EXTRA_ARG");
      // check the result
      std::map<std::string, std::string> tags;
      int column=0;
      int row =0;
      while (row = gc->readTags(column, tags)) {
	// got one row
	if ( !strcmp((tags[ginfo->lfn].c_str()),"1")) {
	  // file commited
	  result = 0;
	}
	column++;
      }
      
      // unlock the UI
      if(!nodelete)delete gc;
    }
  } else {
    return result;
  }


  XrdPosixXrootd::Close(fd);
#endif
  fileDescriptors.releaseFD(fd);
  return result;
}


/** Gapi version of the lseek function call. @fd is a gapi file
    descriptor as returned by the gapi_open call. @offset is the
    offset with respect to @whence (see lseek). Returns the new
    offset in the file or -1 in case of an error in which case errno
    is set to the following error codes:
    - EBADF: The @fd file descriptor is bad
*/
off_t gapi_lseek(int fd, off_t offset, int whence)
{
#ifdef XROOTD
  if (fd<XrdPosixFD)
    return lseek(fd, offset, whence);  // System lseek call
  // does not work for the moment
  return XrdPosixXrootd::Lseek(fd,(long long)offset,whence);
#else
  return -1;
#endif
}


/** Gapi version of the unlink system call. @pathname is the path to
    a file to be deleted. If the path starts with "/alien" it is
    considered to be a path in the catalogue, otherwise it is
    considered to be local and the local unlink() system call is
    called up.
*/
int gapi_unlink(const char *pathname)
{
  // Analyze the pathnmame
  if(urlIsLocal(pathname))
    return unlink(pathname);

  std::string name=pathname;
  urlRemotePart(name);

  std::string command="rm ";
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

  if(!nodelete)delete gc;
  return 0;
}

/** Gapi version of the chmod function call. 
*/
int gapi_chmod(const char *pathname, mode_t mode)
{
  // Analyze the pathnmame
  if(urlIsLocal(pathname))
    return chmod(pathname, mode);

  std::string name=pathname;

  urlRemotePart(name);

  std::stringstream command;

  bool nodelete=false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }
  GapiUI *gc = GapiUI::MakeGapiUI();
  
  command << "chmod ";
  command << std::ios::oct << mode;
  command << " " << name;

  DEBUGMSG(3, << "Sending command: >" << command << "<" << std::endl);

  if(executeRemoteCommand(gc, command.str())){
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

/** Gapi version of the rename system call. 
    @oldpath is renamed to @newpath
*/

int gapi_rename(const char *oldpath, const char *newpath) {
// Analyze the pathnmame
  if(urlIsLocal(oldpath))
    return rename(oldpath, newpath);

  std::string oldname=oldpath;
  std::string newname=newpath;

  urlRemotePart(oldname);
  urlRemotePart(newname);
  std::stringstream command;

  bool nodelete=false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }
  GapiUI *gc = GapiUI::MakeGapiUI();
  
  command << "mv ";
  command << oldname << " " << newname;

  DEBUGMSG(3, << "Sending command: >" << command << "<" << std::endl);

  if(executeRemoteCommand(gc, command.str())){
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


/** Gapi version of the find command.
    @path is the directory tree node, where to start the query.
    @pattern is the match pattern for the query.
    @maxresult limit's the number of results per database query.
    @maxresult=-1 queries without limit!
    - return's -1 in case of error
    - return's the number of found items 
    ( the result's are stored in the result map )
**/

int gapi_find(const char *path, const char *pattern, int maxresult=1000) {
  if(urlIsLocal(path))
    return -1;

  std::string newpath=path;

  urlRemotePart(newpath);
  std::stringstream command;

  bool nodelete=false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }
  GapiUI *gc = GapiUI::MakeGapiUI();

  command << "find -z ";
  if (maxresult != -1) {
    char smaxresult[512];
    sprintf(smaxresult,"%d",maxresult);
    command << "-l " << smaxresult << " ";
  }

  command << newpath << " " << pattern;

  DEBUGMSG(3, << "Sending command: >" << command << "<" << std::endl);

  if(executeRemoteCommand(gc, command.str())){
    if (!nodelete)delete gc;
    return -1;
  }

  return gc->GetStreamColumns(TBytestreamCODEC::kOUTPUT);
}

/** Utility function to get back result's of the find command
    return's a pointer to the character string of the result row field
    or 0, if it does not exist
**/

const char* gapi_getfindresult(const char* tag, int row) {
  bool nodelete = false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }
  GapiUI *gc = GapiUI::MakeGapiUI();
  if (!gc) {
    return 0;
  }

  if (!tag) 
    return 0;

  // check the result
  std::map<std::string, std::string> tags;
  if (gc->readTags(row, tags)) {
    // got one row
    if ( (tags[tag].length() )>0) {
      if(!nodelete)delete gc;
      return tags[tag].c_str();
    }
  }
  if(!nodelete)delete gc;
  return 0;
}


int gapi_copy(int argc, char *argv[], long buffersize) {
  int fin;
  int fout;
  struct timeval abs_start_time;
  struct timeval abs_stop_time;
  struct timezone tz;

  if (argc != 3) {
    return -99;
  }
  
  fin  = gapi_open(argv[1],0);
  fout = gapi_open(argv[2],O_CREAT|O_WRONLY, S_IRWXU | S_IRGRP | S_IROTH);

  if (!fin){
    return -10;
  }
  if (!fout) {
    return -11;
  }

  void* buffer = malloc(buffersize);
  if (!buffer) {
    return -13;
  }

  ssize_t rbytes;
  ssize_t wbytes;
  ssize_t tot_rbytes=0;
  ssize_t tot_wbytes=0;
  int result=0;

  gettimeofday(&abs_start_time,&tz);
  
  while ( (rbytes = gapi_read(fin,buffer, buffersize)) > 0 ) {
    tot_rbytes+= rbytes;
    wbytes = gapi_write(fout, buffer, rbytes);
    if (wbytes>0) {
      tot_wbytes += wbytes;
    }
    if (rbytes != wbytes) {
      result = -20;
      break;
    }

  }
  
  gettimeofday (&abs_stop_time, &tz);
  float abs_time=((float)((abs_stop_time.tv_sec - abs_start_time.tv_sec) *1000 +
			  (abs_stop_time.tv_usec - abs_start_time.tv_usec) / 1000));


  if (!result) {
    COUT(("[gapi_cp] #################################################################\n"));
    COUT(("[gapi_cp] # Source Name              : %s\n",argv[1]));
    COUT(("[gapi_cp] # Destination Name         : %s\n",argv[2]));
    COUT(("[gapi_cp] # Data Copied [bytes]      : %lld\n",tot_wbytes));
    COUT(("[gapi_cp] # Realtime                 : %f\n",abs_time/1000.0));
    if (abs_time > 0) {
      COUT(("[xrdcp] # Eff.Copy. Rate[Mb/s]     : %f\n",tot_wbytes/abs_time/1000.0));
    }
    COUT(("[gapi_cp] #################################################################\n"));
  }

  free(buffer);			      
  return result;
}
