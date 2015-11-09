//          $Id: XrdxAlienFs.cc,v 1.16 2007/10/04 01:34:19 ajp Exp $

const char *XrdxAlienFsCVSID = "$Id: XrdxAlienFs.cc,v 2.0.0 2007/10/04 01:34:19 ajp Exp $";


#include "XrdVersion.hh"
#include "XrdNet/XrdNetDNS.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucTokenizer.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdOuc/XrdOucTList.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysPthread.hh"
#include "XrdSys/XrdSysTimer.hh"
#include "XrdSec/XrdSecInterface.hh"
#include "XrdSfs/XrdSfsAio.hh"
#include "XrdOfs/XrdOfs.hh"
#include "XrdxAlienFs/XrdxAlienFs.hh"
#include "XrdxAlienFs/XrdxAlienTrace.hh"
#include "XrdxAlienFs/XrdxAlienTiming.hh"

#include <uuid/uuid.h>

#ifdef AIX
#include <sys/mode.h>
#endif

#include "config.h"

/******************************************************************************/
/*       O S   D i r e c t o r y   H a n d l i n g   I n t e r f a c e        */
/******************************************************************************/

#ifndef S_IAMB
#define S_IAMB  0x1FF
#endif

/******************************************************************************/
/*                        G l o b a l   O b j e c t s                         */
/******************************************************************************/

XrdSysError     xAlienFsEroute(0);  
XrdSysError    *XrdxAlienFs::eDest;
XrdOucTrace      xAlienFsTrace(&xAlienFsEroute);
XrdxAlienFs* XrdxAlienFS=0;

extern XrdSysError OfsEroute;
extern XrdOss     *XrdOfsOss;

/*----------------------------------------------------------------------------*/
  
/******************************************************************************/
/*                           C o n s t r u c t o r                            */
/******************************************************************************/

XrdxAlienFs::XrdxAlienFs(XrdSysError *ep)
{
  eDest = ep;
  ConfigFN  = 0;  
  codec = 0;
  LastAuthDumpTime = 0;
}

/******************************************************************************/
/*                           I n i t i a l i z a t i o n                      */
/******************************************************************************/
bool
XrdxAlienFs::Init(XrdSysError &ep)
{
  return true;
}

/******************************************************************************/
/*                         G e t F i l e S y s t e m                          */
/******************************************************************************/

extern "C"
{  
XrdSfsFileSystem *XrdSfsGetFileSystem(XrdSfsFileSystem *native_fs, 
                                      XrdSysLogger     *lp,
				      const char       *configfn)
{
  xAlienFsEroute.SetPrefix("xalienfs_");
  xAlienFsEroute.logger(lp);
  
  OfsEroute.SetPrefix("ofs_");
  OfsEroute.logger(lp);
  
  static XrdxAlienFs myFS(&xAlienFsEroute);

  XrdOucString vs="xAlienFs (xAlien File System) v ";
  vs += PACKAGE_VERSION;
  xAlienFsEroute.Say("++++++ (c) 2008 ALICE",vs.c_str());
  
  // Initialize the subsystems
  //
  if (!myFS.Init(xAlienFsEroute) ) return 0;

  myFS.ConfigFN = (configfn && *configfn ? strdup(configfn) : 0);
  if ( myFS.Configure(xAlienFsEroute) ) return 0;

  XrdxAlienFS = &myFS;

  // FIX ME !!! -> port not hardcoded
  XrdxAlienFS->auth = new TSharedAuthentication(true,(const char*)"10000");

  XrdxAlienFS->WorkerSem.CondWait();

  for (int i=0; i< XrdxAlienFS->AlienWorker; i++) {
    XrdxAlienFS->WorkerSem.Post();
    XrdxAlienFS->WorkerOutput[i] = (char*) malloc(DEFAULTRESULTSIZE);
    XrdxAlienFS->WorkerOutputLen[i] = DEFAULTRESULTSIZE;
  }
  XrdxAlienFS->WorkerLast = 0;

  return (XrdOfs*) &myFS;
}
}

/******************************************************************************/
/*                F i l e   O b j e c t   I n t e r f a c e s                 */
/******************************************************************************/

int XrdxAlienFs::Callout(XrdOucErrInfo &error, char** outbyteresult, char *decodedinbytestream, char* uuidstring, int &sid, int &myworker, struct timeval &tv_select, struct timezone &tz) {
  static const char *epname = "callout";
  const char *tident = error.getErrUser();

  char response_uuidstring[40];

  int inuse=0;

  // scan how many workers are in use 
  for (int i=0; i<XrdxAlienFS->AlienWorker; i++) {
    if (XrdxAlienFS->WorkerAssigned[i]) {
      inuse++;
    }
  }
  
  ZTRACE(fsctl,"worker in use: " << inuse << "/" << XrdxAlienFS->AlienWorker);

  // take a free slot
  // wait for a free slot

  XrdxAlienFS->WorkerSem.Wait();
  XrdxAlienFS->WorkerMutex.Lock();
  int mi=0;

  for (int i=0; i<XrdxAlienFS->AlienWorker; i++) {
    mi = (XrdxAlienFS->WorkerLast + i + 1)% XrdxAlienFS->AlienWorker;
    //    printf("Worker %d is %d\n", mi, XrdxAlienFS->WorkerAssigned[mi]);
    if (!XrdxAlienFS->WorkerAssigned[mi]) {
      XrdxAlienFS->WorkerAssigned[mi] = true;
      myworker = mi;
      XrdxAlienFS->WorkerLast = myworker;
      XrdxAlienFS->WorkerStarted[mi] = time(NULL);
      break;
    }
  }

  // regulary update the shm auth stats file - once per minute
  time_t now = time(NULL);
  if ( (now - XrdxAlienFS->LastAuthDumpTime) > 60) {
    char shmauthfile[4096];
    // fix me - PORT 10000 !!!
    sprintf(shmauthfile,"/tmp/shmstat_10000.txt");
    XrdxAlienFS->auth->WriteUserInfoFile(shmauthfile);
    XrdxAlienFS->LastAuthDumpTime = now;
  }


  XrdxAlienFS->WorkerMutex.UnLock();
  
  gettimeofday(&tv_select,&tz);

  if (myworker == -1) {
    //    printf("Myworker is %d\n", myworker);
    // that is really FATAL
    //    printf("fatal error - exiting\n");
    exit(-1);
  } else {
    //    printf("Selected Worker %d\n",myworker);
  }
  // send task to worker
  char zerouser[64];
  memset(zerouser,0,64);
  if (sid!=-1) {
    sprintf(zerouser,"%s",XrdxAlienFS->auth->GetUser(sid));
  } else {
    sprintf(zerouser,"LocalCall");
  }
  int dlen = strlen((char*)decodedinbytestream) + 1;
  
  // we send always 32byte for the user
  int messagelen = dlen;
  //  printf("Messagelen is %d for worker %d\n",messagelen,myworker);

  if ( ( ::write(XrdxAlienFS->AlienXO[myworker]->SockNum(), &messagelen ,sizeof(messagelen))) != sizeof(messagelen)) {
    // write error
    return XrdxAlienFs::Emsg(epname, error, ECONNREFUSED, "execute - worker communcation failed","");
  }

  //  printf("Writing %lld %d %s\n",(long long) XrdxAlienFS, myworker,uuidstring);
  if ( ( ::write(XrdxAlienFS->AlienXO[myworker]->SockNum(), uuidstring, 40)) != 40) {
    // write error
    return XrdxAlienFs::Emsg(epname, error, ECONNREFUSED, "execute - worker communcation failed","");
  }
  
  if ( ( ::write(XrdxAlienFS->AlienXO[myworker]->SockNum(), zerouser, sizeof(zerouser))) != sizeof(zerouser)) {
    // write error
    return XrdxAlienFs::Emsg(epname, error, ECONNREFUSED, "execute - worker communcation failed","");
  }
  
  if ( ( ::write(XrdxAlienFS->AlienXO[myworker]->SockNum(), decodedinbytestream, dlen)) != dlen) {
    // write error
    return XrdxAlienFs::Emsg(epname, error, ECONNREFUSED, "execute - worker communcation failed","");
  }
  
  if (!XrdxAlienFS->AlienXI[myworker]) {
    XrdOucString  xi    = "xao"; xi+= myworker;
    XrdxAlienFS->AlienXI[myworker]  = XrdNetSocket::Create(XrdxAlienFs::eDest, XrdxAlienFS->AlienOutputDir.c_str(), xi.c_str(), S_IRWXU, XRDNET_FIFO );
  }
  
 readagain:
  // receive output from worker
  int result=0;
  //  printf("Receiving output from worker %d %d...\n", myworker, sizeof(result));
  
  if ( ( ::read(XrdxAlienFS->AlienXI[myworker]->SockNum(), &result, sizeof(result)) ) != sizeof(result) ) {
    // read error
    return XrdxAlienFs::Emsg(epname, error, ECONNREFUSED, "execute - worker communcation failed (read)","");
  }
  
  //  printf("Received %d ...\n", result);
  
  // consider the outbyteresult buffer ...
  if ( (result-(512*1024)) > XrdxAlienFS->WorkerOutputLen[myworker]) {
    // we don't allow bigger results than 1 GB
    if (result > (1024*1024*1024)) {
      fprintf(stderr,"fatal: got result of size %d - resetting worker\n", result);
      result = -1;
    } else {
      fprintf(stderr, "reallocating worker %d from %d=>%d\n", myworker, XrdxAlienFS->WorkerOutputLen[myworker], result + (512*1024));

      free( XrdxAlienFS->WorkerOutput[myworker]); 
      XrdxAlienFS->WorkerOutput[myworker] = (char*)malloc(result + (512*1024) );
      XrdxAlienFS->WorkerOutputLen[myworker] = result + (512*1024);
      if (!XrdxAlienFS->WorkerOutput[myworker]) {
        fprintf(stderr, "reallocating failed - out of memory ?\n");
        exit(-1);
      }
    }
  }
  
  
  if ( result > 0) {
    if ( ( ::read(XrdxAlienFS->AlienXI[myworker]->SockNum(), response_uuidstring, 40)) != 40) {
      // read error
      return XrdxAlienFs::Emsg(epname, error, ECONNREFUSED, "execute - worker communcation failed(read response uuid)","");
    }
    *outbyteresult = XrdxAlienFS->WorkerOutput[myworker];
    int rresult=0;
    off_t offset =0;
    errno = 0;
    while ( ( (result-offset) && ( ( rresult=::read(XrdxAlienFS->AlienXI[myworker]->SockNum(), *outbyteresult+offset,result-offset)) >0 ) ) || (errno==EAGAIN) || (errno==EINTR)) { if (rresult>0) {offset+=rresult; }}
    if ( offset != result) {
      // read error
      fprintf(stderr,"I read only %d of %ld %d\n", rresult, offset, errno);
      return XrdxAlienFs::Emsg(epname, error, ECONNREFUSED, "execute - worker communication failed (read)","");
    }
  } else {
    if (result < 0) {
      // this is an error !
      // resize output buffer
      if (XrdxAlienFS->WorkerOutputLen[myworker] != DEFAULTRESULTSIZE) {
	free( XrdxAlienFS->WorkerOutput[myworker]); 
	XrdxAlienFS->WorkerOutput[myworker] = (char*)malloc(DEFAULTRESULTSIZE);
	XrdxAlienFS->WorkerOutputLen[myworker] = DEFAULTRESULTSIZE;
        if (!XrdxAlienFS->WorkerOutput[myworker]) {
          fprintf(stderr, "reallocating failed - out of memory ?\n");
          exit(-1);
        }
      }
      // free worker
      XrdxAlienFS->WorkerMutex.Lock();
      XrdxAlienFS->WorkerAssigned[myworker] = false;
      XrdxAlienFS->WorkerStarted[myworker]  = 0;
      XrdxAlienFS->WorkerMutex.UnLock();
      XrdxAlienFS->WorkerSem.Post();
      //      printf("Posting Sem for %d\n",myworker);
      if (result == (-ETIMEDOUT)) {
	return XrdxAlienFs::Emsg(epname, error, -result, "open - execution failed - command exceeded execution time", "");
      } else {
	return XrdxAlienFs::Emsg(epname, error, -result, "open - execution failed", "");
      }
    }
  }
  if (strncmp(uuidstring,response_uuidstring,strlen(uuidstring))) {
    // this is not the response we were waiting for ... just skip it and read the next one ....
    fprintf(stderr, "Upps -- received response for %s ... but I am waiting for %s - skip this one\n",response_uuidstring, uuidstring);
    goto readagain;
  }
  return 0;
}


/******************************************************************************/
/*                                  o p e n                                   */
/******************************************************************************/

int XrdxAlienFsFile::open(const char          *path,      // In
                           XrdSfsFileOpenMode   open_mode, // In
                           mode_t               Mode,      // In
                     const XrdSecEntity        *client,    // In
                     const char                *info)      // In
/*
  Function: Open the file `path' in the mode indicated by `open_mode'.  

  Input:    path      - The fully qualified name of the file to open.
            open_mode - One of the following flag values:
                        SFS_O_RDONLY - Open file for reading.
                        SFS_O_WRONLY - Open file for writing.
                        SFS_O_RDWR   - Open file for update
                        SFS_O_CREAT  - Create the file open in RDWR mode
                        SFS_O_TRUNC  - Trunc  the file open in RDWR mode
            Mode      - The Posix access mode bits to be assigned to the file.
                        These bits correspond to the standard Unix permission
                        bits (e.g., 744 == "rwxr--r--"). Mode may also conatin
                        SFS_O_MKPTH is the full path is to be created. The
                        agument is ignored unless open_mode = SFS_O_CREAT.
            client    - Authentication credentials, if any.
            info      - Opaque information to be used as seen fit.

  Output:   Returns OOSS_OK upon success, otherwise SFS_ERROR is returned.
*/
{
   static const char *epname = "open";
   const char *tident = error.getErrUser();

   gettimeofday(&tv_open,&tz);

   XrdxAlienTiming opentiming("fileopen");
   
   XTRACE(open, path,"");

   TIMING(xAlienFsTrace,"START",&opentiming);
   XrdOucEnv env(info);


   fname = XrdxAlienFS->AlienOutputDir;
   fname += "/";

   ///////////////////////////////////////////////////////////////////////
   // do something here and return SFS_OK if ok, otherwise use Emsg 
   // and set errno to some useful value
   // return XrdxAlienFs::Emsg(epname, error, errno, "open", fname.c_str());

  const char* scmd;
  ZTRACE(open,info);

  // create a GUID
  XrdxAlienFS->UuidMutex.Lock();
  uuid_t newuuid;
  
  uuid_generate_time(newuuid);
  char uuidstring[40];
  memset(uuidstring,0,sizeof(uuidstring));

  uuid_unparse(newuuid,uuidstring);
  XrdxAlienFS->UuidMutex.UnLock();
  fname += uuidstring;

  XrdOucString answer="";

  if ((scmd = env.Get("xalien.cmd"))) {
    if (!strcmp(scmd, "execute")) {
      int sid=0;
      // execute
      answer+= "&xalien.session_id=";
      answer+= env.Get("xalien.session_id");

      if (!env.Get("xalien.session_id")) {
	return XrdxAlienFs::Emsg(epname, error, EINVAL, "execute - session ID missing", "");
      } else {
	sid = atoi(env.Get("xalien.session_id"));
      }
      
      if (!env.Get("xalien.magic")) {
	return XrdxAlienFs::Emsg(epname, error, EINVAL, "execute - magic token missing", "");
      }

      //      printf("Get Context for session id %d\n",sid);
      //      printf("Decoding %s\n",env.Get("xalien.magic"));
      SPC_CIPHERQ *q=XrdxAlienFS->auth->GetCipherContext(sid);
     
      size_t dectokenlen;
      XrdxAlienFS->CodecMutex.Lock();
      char *magic_token=(char *)
	spc_cipherq_b64decrypt(q,(unsigned char*) env.Get("xalien.magic"),
			       &dectokenlen);
      XrdxAlienFS->CodecMutex.UnLock();
      if(!magic_token)
	return XrdxAlienFs::Emsg(epname, error, EINVAL, "execute - session invalid - cannot decrypt magic token","");
      

      ZTRACE(fsctl,"decrypted magic token: >" << magic_token
	     << "< length: " << dectokenlen << "  "  << strlen(magic_token));
      
      if (strcmp(magic_token,AUTHMAGIC)) {
	free(magic_token);
	return XrdxAlienFs::Emsg(epname, error, EINVAL, "execute - magic token does not match","");
      }
      
      free(magic_token);
      
      XrdxAlienFS->auth->MarkAccess(sid);
      if ( XrdxAlienFS->auth->GetLifetime(sid)<=0) {
	return XrdxAlienFs::Emsg(epname, error, ETIME, "execute - session is expired","");
      }

      // decrypt the command input
      char* inbytestream=0;
      if (!(inbytestream = env.Get("xalien.inputstream"))) {
	return XrdxAlienFs::Emsg(epname, error, EINVAL, "execute - no input stream exists","");
      }
      
      ZTRACE(open, "Encrypted command:>" << inbytestream << "  length:" << strlen(inbytestream));
 
      ZTRACE(open, "DECRYPTING CLIENT'S COMMAND");
      size_t decodedlen;
      XrdxAlienFS->CodecMutex.Lock();
      unsigned char *decodedinbytestream=
	spc_cipherq_b64decrypt(q,(unsigned char*) inbytestream,
			       &decodedlen);
      XrdxAlienFS->CodecMutex.UnLock();
      if(!decodedinbytestream) {
	return XrdxAlienFs::Emsg(epname, error, EINVAL, "execute - cannot decrypt your command","");
      }
      
      ZTRACE(open, "Decrypted command:>" << decodedinbytestream );
      
      gapi_envhash_t *envhash=getEnvHash((char*)decodedinbytestream);
      if(!envhash) {
	std::cerr << "failed to extract envhash\n";
	exit(2);
      }
      
      XrdxAlienFS->auth->IncrementCmdcnt(sid);
      XrdxAlienFS->auth->IncrementCommandTotal();


      answer+="&xalien.magic=";
      // encode the return magic
      magic_token = new char[strlen(AUTHMAGIC_CL)+1];
      
      strcpy(magic_token,AUTHMAGIC_CL);
      XrdxAlienFS->CodecMutex.Lock();
      char *magic_crypto= (char*)
	spc_cipherq_b64encrypt(q,(unsigned char *)magic_token,
			       strlen(magic_token));
      XrdxAlienFS->CodecMutex.UnLock();
      ZTRACE(open,"encrypted magic token is" << magic_crypto);
      if (magic_token) 
	delete [] magic_token;
      answer+=magic_crypto;
      if (magic_crypto) free(magic_crypto);
      std::string outbytestream="";
      char* outbyteresult=(char*)"";

      // do the communication here
      int myworker=-1;

      // here insert the callout

      int rc = XrdxAlienFS->Callout(error, (char**) &outbyteresult, (char*)decodedinbytestream, (char*)uuidstring, sid, myworker, tv_select, tz);
      gettimeofday(&tv_called,&tz);
      
      free(decodedinbytestream);

      if (rc) {	return rc; }

      //      printf("Outbyteresult = %s\n",outbyteresult);

      if(envhash->find("encryptReply")!=envhash->end()) {
	ZTRACE(open, "ENCRYPTING COMMAND OUTPUT");
	XrdxAlienFS->CodecMutex.Lock();
	char *encoded= (char*)
	  spc_cipherq_b64encrypt(q,(unsigned char *)outbyteresult,
				 strlen(outbyteresult));
	XrdxAlienFS->CodecMutex.UnLock();
	if (encoded) {
	  outbytestream=encoded;
	  free(encoded);
	}
      } else {
	outbytestream = outbyteresult;
      }
      answer+="&xalien.outputstream=";
      answer+=outbytestream.c_str();
      
      TSharedAuthentication::IncrementNonce(q->nonce);

      delete envhash;

      // resize output buffer
      if (XrdxAlienFS->WorkerOutputLen[myworker] != DEFAULTRESULTSIZE) {
	fprintf(stderr, "reallocating worker %d from %d=>%d\n", myworker, XrdxAlienFS->WorkerOutputLen[myworker], DEFAULTRESULTSIZE);
	free(XrdxAlienFS->WorkerOutput[myworker]);
	
	XrdxAlienFS->WorkerOutput[myworker] = (char*)malloc(DEFAULTRESULTSIZE);
	XrdxAlienFS->WorkerOutputLen[myworker] = DEFAULTRESULTSIZE;
        if (!XrdxAlienFS->WorkerOutput[myworker]) {
          fprintf(stderr, "reallocating failed - out of memory ?\n");
          exit(-1);
        }
      }
      // free worker
      XrdxAlienFS->WorkerMutex.Lock();
      XrdxAlienFS->WorkerAssigned[myworker] = false;
      XrdxAlienFS->WorkerStarted[myworker]  = 0;
      XrdxAlienFS->WorkerSem.Post();
      XrdxAlienFS->WorkerMutex.UnLock();
      //      printf("Posting Sem for %d\n",myworker);
    }
  } else {
    return XrdxAlienFs::Emsg(epname, error, EINVAL, "open - no command specified", "");
  }

  int rc = XrdOfsFile::open(fname.c_str(),open_mode | SFS_O_CREAT|SFS_O_TRUNC|SFS_O_RDWR,Mode ,client,info);
  if (rc == SFS_OK) {
    executed=true;
    if ( (XrdOfsFile::write(0, answer.c_str(), answer.length()+1)) != (answer.length()+1)) {
      ZTRACE(open,"error=" << rc);
      rc = SFS_ERROR;
      return XrdxAlienFs::Emsg(epname, error, EIO, "execute - result file creation failed","");
    } else 
      rc = SFS_OK;
  }
  
  TIMING(xAlienFsTrace,"DONE",&opentiming);
  opentiming.Print(xAlienFsTrace);
  ZTRACE(open,"retc=" << rc);

  gettimeofday(&tv_opened,&tz);
  return rc;
}

/******************************************************************************/
/*                                 c l o s e                                  */
/******************************************************************************/

int XrdxAlienFsFile::close()
/*
  Function: Close the file object.

  Input:    None

  Output:   Returns SFS_OK upon success and SFS_ERROR upon failure.
*/
{
   static const char *epname = "close";
   const char *tident = error.getErrUser();

   //   static const char *epname = "close";
   if (executed) {
     XrdOfsOss->Unlink(fname.c_str());
   }
   fname = "";
   gettimeofday(&tv_closed,&tz);

   char timings[16384];
   sprintf(timings,"select=%.02f call=%.02f open=%.02f transaction=%.02f", 
	   ( ((tv_select.tv_sec - tv_open.tv_sec)*1000.0) + ((tv_select.tv_usec - tv_open.tv_usec)/1000.0)),
	   ( ((tv_called.tv_sec - tv_select.tv_sec)*1000.0) + ((tv_called.tv_usec - tv_select.tv_usec)/1000.0)),
	   ( ((tv_opened.tv_sec - tv_open.tv_sec)*1000.0) + ((tv_opened.tv_usec - tv_open.tv_usec)/1000.0)),
	   ( ((tv_closed.tv_sec - tv_open.tv_sec)*1000.0) + ((tv_closed.tv_usec - tv_open.tv_usec)/1000.0)));

   ZTRACE(open,"timings: " << timings);
   return SFS_OK;
}

/******************************************************************************/
/*                            g e t V e r s i o n                             */
/******************************************************************************/

const char *XrdxAlienFs::getVersion() {return XrdVERSION;}



/******************************************************************************/
/*                                  E m s g                                   */
/******************************************************************************/

int XrdxAlienFs::Emsg(const char    *pfx,    // Message prefix value
                       XrdOucErrInfo &einfo,  // Place to put text & error code
                       int            ecode,  // The error code
                       const char    *op,     // Operation being performed
                       const char    *target) // The target (e.g., fname.c_str())
{
    char *etext, buffer[4096], unkbuff[64];

// Get the reason for the error
//
   if (ecode < 0) ecode = -ecode;
   if (!(etext = strerror(ecode)))
      {sprintf(unkbuff, "reason unknown (%d)", ecode); etext = unkbuff;}

// Format the error message
//
    snprintf(buffer,sizeof(buffer),"Unable to %s %s; %s", op, target, etext);

// Print it out if debugging is enabled
//
#ifndef NODEBUG
    eDest->Emsg(pfx, buffer);
#endif

// Place the error message in the error object and return
//
    einfo.setErrInfo(ecode, buffer);

    return SFS_ERROR;
}


/******************************************************************************/
/*                                 S t a l l                                  */
/******************************************************************************/

int XrdxAlienFs::Stall(XrdOucErrInfo   &error, // Error text & code
                  int              stime, // Seconds to stall
                  const char      *msg)   // Message to give
{
  XrdOucString smessage = msg;
  smessage += "; come back in ";
  smessage += stime;
  smessage += " seconds!";
  
  EPNAME("Stall");
  const char *tident = error.getErrUser();
  
  ZTRACE(delay, "Stall " <<stime <<": " << smessage.c_str());

  // Place the error message in the error object and return
  //
  error.setErrInfo(0, smessage.c_str());
  
  // All done
  //
  return stime;
}


int            
XrdxAlienFs::FSctl(const int               cmd,
		   XrdSfsFSctl            &args,
		   XrdOucErrInfo          &error,
		   const XrdSecEntity     *client)
  
{  
  static const char *epname = "FSctl";
  const char *tident = error.getErrUser(); 
  
  char ipath[4096];
  char iopaque[4096];

  struct timeval tv;
  struct timezone tz;

  if (cmd!=SFS_FSCTL_PLUGIN) {
    return SFS_ERROR;
  }
  
  ZTRACE(fsctl,"len1="<<args.Arg1Len);
  ZTRACE(fsctl,"len2="<<args.Arg2Len);
  if (args.Arg1Len) {
    strncpy(ipath,args.Arg1,args.Arg1Len);
    ipath[args.Arg1Len] = 0;
  } else {
    ipath[0] = 0;
  }
  if (args.Arg2Len) {
    strncpy(iopaque,args.Arg2,args.Arg2Len);
    iopaque[args.Arg2Len] = 0;
  } else {
    iopaque[0] = 0;
  }
  
  XrdOucString path = ipath;
  XrdOucString opaque = iopaque;
  
  XrdOucEnv env(opaque.c_str());
  const char* scmd;

  ZTRACE(fsctl,opaque.c_str());

  if ((scmd = env.Get("xalien.cmd"))) {
    //
    char* authorizedrole = env.Get("xalien.user");

    if (!strcmp(scmd, "getToken")) {
      XrdOucString answer="";
      // gettoken 
      
      // verify the user subject
      XrdOucString alienuser = env.Get("xalien.user");
      XrdOucString alienjobtoken = env.Get("xalien.jobtoken");
      
      if ( alienuser.beginswith("jobtoken:") && alienjobtoken.length()) {
	XrdOucString jobid=alienuser;
	jobid.erase(0,9);
	ZTRACE(fsctl,"got jobid: " << jobid.c_str());
	
	char* outbyteresult=0;
	XrdOucString sdecodedinbytestream="verifyToken:"; sdecodedinbytestream += jobid; sdecodedinbytestream+=":"; sdecodedinbytestream+=alienjobtoken;
	char* decodedinbytestream = (char*)sdecodedinbytestream.c_str();
	
	uuid_t newuuid;
	XrdxAlienFS->UuidMutex.Lock();
	uuid_generate_time(newuuid);
	char uuidstring[40];
        memset(uuidstring,0,sizeof(uuidstring));

	uuid_unparse(newuuid,uuidstring);
	XrdxAlienFS->UuidMutex.UnLock();
	int sid=-1;
	int myworker = -1;
	ZTRACE(fsctl,"in: " << decodedinbytestream);
	int rc = XrdxAlienFS->Callout(error, (char**) &outbyteresult, (char*)decodedinbytestream, (char*)uuidstring, sid, myworker, tv, tz);
	if (rc!=0) return rc;
	
	if (!outbyteresult) {
	  // free worker
	  XrdxAlienFS->WorkerMutex.Lock();
	  XrdxAlienFS->WorkerAssigned[myworker] = false;
	  XrdxAlienFS->WorkerStarted[myworker]  = 0;
	  XrdxAlienFS->WorkerMutex.UnLock();
	  XrdxAlienFS->WorkerSem.Post();
	  return XrdxAlienFs::Emsg(epname, error, EPERM, "authenticate - cannot verify token", "");
	}
	
	alienuser = outbyteresult;
	
	// resize output buffer
	if (XrdxAlienFS->WorkerOutputLen[myworker] != DEFAULTRESULTSIZE) {
	  fprintf(stderr, "reallocating worker %d from %d=>%d\n", myworker, XrdxAlienFS->WorkerOutputLen[myworker], DEFAULTRESULTSIZE);
	  free(XrdxAlienFS->WorkerOutput[myworker]);
	  XrdxAlienFS->WorkerOutput[myworker] = (char*)malloc(DEFAULTRESULTSIZE);
	  XrdxAlienFS->WorkerOutputLen[myworker] = DEFAULTRESULTSIZE;
          if (!XrdxAlienFS->WorkerOutput[myworker]) {
            fprintf(stderr, "reallocating failed - out of memory ?\n");
            exit(-1);
          }
	}
	// free worker
	XrdxAlienFS->WorkerMutex.Lock();
	XrdxAlienFS->WorkerAssigned[myworker] = false;
	XrdxAlienFS->WorkerStarted[myworker]  = 0;
	XrdxAlienFS->WorkerMutex.UnLock();
	XrdxAlienFS->WorkerSem.Post();
	//	printf("Posting Sem for %d\n",myworker);
	
	if (!strlen(outbyteresult)) {
	  return XrdxAlienFs::Emsg(epname, error, EPERM, "authenticate - invalid job token", "");
	}
	
	authorizedrole = (char*) alienuser.c_str();

	alienjobtoken="";
	ZTRACE(fsctl,"authenticated "<<  env.Get("xalien.user") << " with token " <<  env.Get("xalien.jobtoken") << " as role: >" << authorizedrole <<"<");
      } else {
	// check GSI(SSL) authentication
	if ( (!strncmp(client->prot,"ssl",3)) || (!strncmp(client->prot,"gsi",3))) {
	  // this is SSL/GSI authenticated
	} else {
	  return XrdxAlienFs::Emsg(epname, error, EPERM, "authenticate - you didn't connect via SSL/GSI!", "");
	}

	char* outbyteresult=0;
	XrdOucString clientdn=client->name;
	int p1 = clientdn.rfind("/CN=");
	if ( (!strncmp(client->prot,"ssl",3)) ) {
	  if (p1 != STR_NPOS) {
	    clientdn.erase(p1);
	  }
	}

	XrdOucString sdecodedinbytestream="verifySubjectRole:"; sdecodedinbytestream += alienuser; sdecodedinbytestream+=":"; sdecodedinbytestream+=clientdn;
	char* decodedinbytestream = (char*)sdecodedinbytestream.c_str();
	
	uuid_t newuuid;
	XrdxAlienFS->UuidMutex.Lock();
	uuid_generate_time(newuuid);
	char uuidstring[40];
	uuid_unparse(newuuid,uuidstring);
	XrdxAlienFS->UuidMutex.UnLock();
	int sid=-1;
	int myworker = -1;
	ZTRACE(fsctl,"in: " << decodedinbytestream);
	int rc = XrdxAlienFS->Callout(error, (char**) &outbyteresult, (char*)decodedinbytestream, (char*)uuidstring, sid, myworker, tv, tz);
	if (rc!=0) return rc;
	
	if (!outbyteresult) {
	  return XrdxAlienFs::Emsg(epname, error, EPERM, "authenticate - cannot verify role", "");
	}
	
	alienuser = outbyteresult;
	
	// resize output buffer
	if (XrdxAlienFS->WorkerOutputLen[myworker] != DEFAULTRESULTSIZE) {
	  fprintf(stderr, "reallocating worker %d from %d=>%d\n", myworker, XrdxAlienFS->WorkerOutputLen[myworker], DEFAULTRESULTSIZE);
	  free(XrdxAlienFS->WorkerOutput[myworker]);
	  XrdxAlienFS->WorkerOutput[myworker] = (char*)malloc(DEFAULTRESULTSIZE);
	  XrdxAlienFS->WorkerOutputLen[myworker] = DEFAULTRESULTSIZE;
          if (!XrdxAlienFS->WorkerOutput[myworker]) {
            fprintf(stderr, "reallocating failed - out of memory ?\n");
            exit(-1);
          }
	}
	// free worker
	XrdxAlienFS->WorkerMutex.Lock();
	XrdxAlienFS->WorkerAssigned[myworker] = false;
	XrdxAlienFS->WorkerStarted[myworker]  = 0;
	XrdxAlienFS->WorkerMutex.UnLock();
	XrdxAlienFS->WorkerSem.Post();
	//	printf("Posting Sem for %d\n",myworker);

	
	if (!strlen(outbyteresult)) {
	  return XrdxAlienFs::Emsg(epname, error, EPERM, "authenticate - cannot grant the requested role to your DN", "");
	}
	
	authorizedrole = (char*) alienuser.c_str();
	
      }

      // check if this is a job token or we have to look at the GSI context
      
     
      // get a new session i
      XrdxAlienFS->CodecMutex.Lock();
      int mysid = auth->Getsid();
      XrdxAlienFS->CodecMutex.UnLock();
      ZTRACE(fsctl, "assigned session id >" << mysid);
      
      answer += "&xalien.session_id=";
      answer += mysid;

      auth->MarkAccess(mysid);
      // set the IP address in the SHM authentication map
      struct sockaddr InetAddr;
      if (XrdNetDNS::getHostAddr(client->host, InetAddr)) {
	auth->SetIp(mysid, htonl(XrdNetDNS::IPAddr(&InetAddr)));
      } else {
	auth->SetIp(mysid,0);
      }

      answer += "&xalien.session_expiretime="; answer += (int)(auth->GetLifetime(mysid) + time(NULL));
      //      auth->Dumpsids();

      unsigned char newkey[SPC_DEFAULT_KEYSIZE];
      spc_keygen(newkey,SPC_DEFAULT_KEYSIZE);
      char *b64newkey = (char*) spc_base64_encode(newkey,SPC_DEFAULT_KEYSIZE,0);
      answer += "&xalien.session_passwd=";
      answer += b64newkey;
      
      ZTRACE(fsctl, "b64 encoded session key: >" << b64newkey );

      if (b64newkey) free(b64newkey);

      
      spc_cipherq_setup(auth->GetCipherContext(mysid),
			newkey,SPC_DEFAULT_KEYSIZE,0);
      
      unsigned char newnonce[SHMAUTH_NONCE_SIZE];
      auth->CreateNonce(newnonce);
      spc_cipherq_setnonce(auth->GetCipherContext(mysid),newnonce);
      
      if (!auth->SetUser(mysid,authorizedrole)) {
	return XrdxAlienFs::Emsg(epname, error, EINVAL, "execute - wrong response for user authorization received", "");
      }

      answer += "&xalien.session_user=";
      answer += authorizedrole;
      char *noncebuffer = TSharedAuthentication::NonceToString(newnonce);
      answer += "&xalien.nonce=";
      answer += noncebuffer;
      free(noncebuffer);
      error.setErrInfo(answer.length()+1, answer.c_str());
      return SFS_DATA;
    }
  } else {
    return XrdxAlienFs::Emsg(epname, error, EINVAL, "execute - no command specified", "");
  }
  
  error.setErrInfo(6,"xAlien");
  return SFS_DATA;
}

gapi_envhash_t *
XrdxAlienFsFile::getEnvHash(char* decodedstr) {
  static const char *epname = "getEnvHash";
  const char *tident = "-";

  XrdxAlienFS->CodecMutex.Lock();
  if(!XrdxAlienFS->codec)
    XrdxAlienFS->codec = new TBytestreamCODEC();


  gapi_envhash_t *envhash = new gapi_envhash_t;
  char *tmp_decode=strdup(decodedstr);
  if(!tmp_decode) {
    XrdxAlienFS->CodecMutex.UnLock();
    return NULL;
  }
  
  if(!XrdxAlienFS->codec->Decode(tmp_decode,strlen(tmp_decode))) {
    XrdxAlienFS->CodecMutex.UnLock();
    return NULL;
  } else {
    for(int i=0;i<XrdxAlienFS->codec->GetStreamField(XrdxAlienFS->codec->kENVIR,0,
					TBytestreamCODEC::kFieldname);i++) {
      ZTRACE(open,XrdxAlienFS->codec->GetFieldName(XrdxAlienFS->codec->kENVIR,0,i) << " => " << XrdxAlienFS->codec->GetFieldValue(XrdxAlienFS->codec->kENVIR,0,i));
      envhash->insert(std::make_pair(XrdxAlienFS->codec->GetFieldName(XrdxAlienFS->codec->kENVIR,0,i),
				     XrdxAlienFS->codec->GetFieldValue(XrdxAlienFS->codec->kENVIR,0,i)));
    }
  }
  free(tmp_decode);
  XrdxAlienFS->CodecMutex.UnLock();
  return envhash;
}
