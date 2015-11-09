//         $Id: XrdxAlienFs.hh,v 1.13 2007/10/04 01:34:19 abh Exp $

#ifndef __XALIEN_FS_H__
#define __XALIEN_FS_H__

#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <utime.h>
#include <pwd.h>

#include "XrdSfs/XrdSfsInterface.hh"
#include "XrdOuc/XrdOucHash.hh"
#include "XrdOuc/XrdOucTable.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdOfs/XrdOfs.hh"
#include "XrdSec/XrdSecEntity.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSys/XrdSysSemWait.hh"
#include "XrdNet/XrdNetSocket.hh"
#include "XrdNet/XrdNetOpts.hh"

#include <map>
#include <string>
#include "CODEC/CODEC.h"
#include "ShmAuth/ShmAuth.h"
#include "cwccrypt/spc_cypherq.h"
#include "cwccrypt/spc_helpers.h"
#include "cwccrypt/spc_b64.h"

#define DEFAULTRESULTSIZE 8*1024*1024

typedef std::map<std::string,std::string> gapi_envhash_t;


class XrdSysError;
class XrdSysLogger;

/******************************************************************************/
/*            U n i x   F i l e   S y s t e m   I n t e r f a c e             */
/******************************************************************************/


/******************************************************************************/
/*                 X r d A l i e n F s D i r e c t o r y                      */
/******************************************************************************/
 
class XrdxAlienFsDirectory : public XrdOfsDirectory
{
public:

  int         open(const char              *dirName,
		   const XrdSecClientName  *client = 0,
		   const char              *opaque = 0) { return SFS_ERROR;}
  
  const char *nextEntry() { return NULL;}
  
  int         close() {return 0;}
  
  const   char       *FName() {return (const char *)fname;}
  
  XrdxAlienFsDirectory(char *user=0) : XrdOfsDirectory(user)
  {ateof = 0; fname = 0;
  dh    = (DIR *)0;
  d_pnt = &dirent_full.d_entry;
  }
  
  ~XrdxAlienFsDirectory() {if (dh) close();}
private:
  
  DIR           *dh;  // Directory stream handle
  char           ateof;
  char          *fname;
  XrdOucString   entry;
  
  struct {struct dirent d_entry;
    char   pad[MAXNAMLEN];   // This is only required for Solaris!
  } dirent_full;
  
  struct dirent *d_pnt;
  
};

/******************************************************************************/
/*                      X r d A l i e n F s F i l e                           */
/******************************************************************************/
  
class XrdSfsAio;


class XrdxAlienFsFile : public XrdOfsFile
{
public:

        int            open(const char                *fileName,
                                  XrdSfsFileOpenMode   openMode,
                                  mode_t               createMode,
                            const XrdSecEntity        *client = 0,
                            const char                *opaque = 0);

        int            close();

        const char    *FName() {return fname.c_str();}


  int            Fscmd(const char* path,  const char* path2, const char* orgipath, const XrdSecEntity *client,  XrdOucErrInfo &error, const char* info) { return SFS_ERROR;}

  int            getMmap(void **Addr, off_t &Size)
  {if (Addr) Addr = 0; Size = 0; return SFS_OK;}
  
  int            read(XrdSfsFileOffset   fileOffset,
		      XrdSfsXferSize     preread_sz) {return SFS_OK;}
  
  XrdSfsXferSize read(XrdSfsFileOffset   fileOffset,
		      char              *buffer,
    XrdSfsXferSize     buffer_size) { return XrdOfsFile::read(fileOffset,buffer,buffer_size);}
  
  int            read(XrdSfsAio *aioparm) { return XrdOfsFile::read(aioparm); }
  
  XrdSfsXferSize write(XrdSfsFileOffset   fileOffset,
		       const char        *buffer,
		       XrdSfsXferSize     buffer_size) { return SFS_ERROR;}
  
  int            write(XrdSfsAio *aioparm) { return SFS_ERROR; }
  
  int            sync() { return SFS_ERROR;}
  
  int            sync(XrdSfsAio *aiop) { return SFS_OK;}

  int            stat(struct stat *buf) { return XrdOfsFile::stat(buf);}
  
  int            truncate(XrdSfsFileOffset   fileOffset) { return SFS_ERROR;}
  
  int            getCXinfo(char cxtype[4], int &cxrsz) {return cxrsz = 0;}
  
  int            fctl(int, const char*, XrdOucErrInfo&) {return 0;}


  gapi_envhash_t* getEnvHash(char* decodedstr);
  
  XrdxAlienFsFile(char *user=0) : XrdOfsFile(user)
  {fname = "";executed=false;}
  ~XrdxAlienFsFile() { close();}
private:
  XrdOucString fname;
  bool executed;
  struct timezone tz;
  struct timeval tv_open;
  struct timeval tv_select;
  struct timeval tv_called;
  struct timeval tv_opened;
  struct timeval tv_closed;
};


/******************************************************************************/
/*                          X r d C a t a l o g F s                           */
/******************************************************************************/
  
class XrdxAlienFs : public XrdOfs
{
  friend class XrdxAlienFsFile;
  friend class XrdxAlienFsDirectory;
  
public:
  // codec used to (de)serialize enquiries and result structures.
  //     only needed because the server needs access to the environment hash
  //     sent with client's query
  XrdSysMutex       CodecMutex;
  XrdSysMutex       UuidMutex;
  TBytestreamCODEC *codec;

  TSharedAuthentication* auth;

  XrdOucString    AlienOutputDir;
  XrdOucString    AlienPerlModule;
  int             AlienWorker;
  
  XrdNetSocket*    AlienXO[100];
  XrdNetSocket*    AlienXI[100];

  XrdSysSemWait    WorkerSem;
  XrdSysMutex      WorkerMutex;
  bool             WorkerAssigned[100];
  time_t           WorkerStarted[100];
  char*            WorkerOutput[100];
  int              WorkerOutputLen[100];
  int              WorkerLast;

  time_t           LastAuthDumpTime;

  // Object Allocation Functions
  //
  XrdSfsDirectory *newDir(char *user=0)
  {return (XrdSfsDirectory *)new XrdxAlienFsDirectory(user);}
  
  XrdSfsFile      *newFile(char *user=0)
  {return      (XrdSfsFile *)new XrdxAlienFsFile(user);}
  
  // Other Functions
  //

  int          Callout(XrdOucErrInfo &error, char** outbyteresult, char *decodedinbytestream, char *uuidstring, int &sid, int &myworker, struct timeval &tv, struct timezone &tz);



  int            chmod(const char             *Name,
		       XrdSfsMode        Mode,
		       XrdOucErrInfo    &out_error,
		       const XrdSecEntity *client = 0,
		       const char             *opaque = 0) { return SFS_ERROR;}
  
  int            exists(const char                *fileName,
			XrdSfsFileExistence &exists_flag,
			XrdOucErrInfo       &out_error,
			const XrdSecEntity    *client = 0,
			const char                *opaque = 0) { return SFS_ERROR;}
  
  int            fsctl(const int               cmd,
		       const char             *args,
		       XrdOucErrInfo    &out_error,
		       const XrdSecEntity *client = 0) { return SFS_ERROR;}
  
  int            getStats(char *buff, int blen) {return 0;}
  
  const   char          *getVersion();
  
  
  int            mkdir(const char             *dirName,
		       XrdSfsMode        Mode,
		       XrdOucErrInfo    &out_error,
		       const XrdSecClientName *client = 0,
		       const char             *opaque = 0) { return SFS_ERROR;}
  
  int            prepare(      XrdSfsPrep       &pargs,
			       XrdOucErrInfo    &out_error,
			       const XrdSecEntity *client = 0) { return SFS_ERROR;}
  
  int            rem(const char             *path,
		     XrdOucErrInfo    &out_error,
		     const XrdSecEntity *client = 0,
		     const char             *opaque = 0) { return SFS_ERROR;}
  
  int            remdir(const char             *dirName,
			XrdOucErrInfo    &out_error,
			const XrdSecEntity *client = 0,
			const char             *opaque = 0) { return SFS_ERROR;}
  
  int            rename(const char             *oldFileName,
			const char             *newFileName,
			XrdOucErrInfo    &out_error,
			const XrdSecEntity *client = 0,
			const char             *opaqueO = 0,
			const char             *opaqueN = 0) { return SFS_ERROR;}
  
  int            stat(const char             *Name,
		      struct stat      *buf,
		      XrdOucErrInfo    &out_error,
		      const XrdSecEntity *client = 0,
		      const char             *opaque = 0) { printf("Calling stat\n");return SFS_ERROR;}
  
  int            lstat(const char            *Name,
		       struct stat      *buf,
		       XrdOucErrInfo    &out_error,
		       const XrdSecEntity *client = 0,
		       const char             *opaque = 0) { return SFS_ERROR;}
  
  int            stat(const char             *Name,
		      mode_t           &mode,
		      XrdOucErrInfo    &out_error,
		      const XrdSecEntity     *client = 0,
		      const char             *opaque = 0) {printf("Calling stat\n"); return SFS_ERROR;}
  
  int            truncate(const char*, XrdSfsFileOffset, XrdOucErrInfo&, const XrdSecEntity*, const char*) { return SFS_ERROR;}
  
  
  int            symlink(const char*, const char*, XrdOucErrInfo&, const XrdSecEntity*, const char*) { return SFS_ERROR;}

  int            readlink(const char*, XrdOucString& ,  XrdOucErrInfo&, const XrdSecEntity*, const char*) { return SFS_ERROR;}
  
  int            access(const char*, int mode, XrdOucErrInfo&, const XrdSecEntity*, const char*) { return SFS_ERROR;}
  
  int            utimes(const char*, struct timeval *tvp, XrdOucErrInfo&, const XrdSecEntity*, const char*) { return SFS_ERROR;}
  
  // Common functions
  //
  static  int            Mkpath(const char *path, mode_t mode, 
				const char *info=0, XrdSecEntity* client = NULL, XrdOucErrInfo* error = NULL) { return SFS_ERROR;}
  
  static  int            Emsg(const char *, XrdOucErrInfo&, int, const char *x,
			      const char *y="");

  int                    FSctl(const int               cmd,
			       XrdSfsFSctl            &args,
			       XrdOucErrInfo    &out_error,
			       const XrdSecEntity *client = 0);


  XrdxAlienFs(XrdSysError *lp);
  virtual               ~XrdxAlienFs() {}
  
  virtual int            Configure(XrdSysError &);
  virtual bool           Init(XrdSysError &);
  int            Stall(XrdOucErrInfo &error, int stime, const char *msg);
  

protected:

private:

static  XrdSysError *eDest;
  
};
#endif
