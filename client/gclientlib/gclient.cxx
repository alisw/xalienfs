#include "XrdOuc/XrdOucString.hh"
#include "XrdOuc/XrdOucEnv.hh"
#include "XrdOuc/XrdOucRash.hh"
#include "XrdClient/XrdClientConst.hh"
#include "XrdClient/XrdClientEnv.hh"
#include "XrdClient/XrdClient.hh"
#include "XrdClient/XrdClientAdmin.hh"

#include <stdlib.h>
#include <stdio.h>
#include <sys/file.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "gclient.h"
#include "gapiUI.h"

#include "../cwccrypt/spc_cypherq.h"
#include "../cwccrypt/spc_b64.h"

#include <iostream>
#include <sstream>
#include <sys/time.h>
#include <fstream>

#ifdef HAVE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#include <curses.h>
#include <term.h>
#endif

#include "../CODEC/CODEC.h"

#define DOENV \
  int DOENV_x1 = EnvGetLong(NAME_CONNECTTIMEOUT); EnvPutInt(NAME_CONNECTTIMEOUT,10); \
  int DOENV_x2 = EnvGetLong(NAME_REQUESTTIMEOUT); EnvPutInt(NAME_REQUESTTIMEOUT,300); \
  int DOENV_x3 = EnvGetLong(NAME_MAXREDIRECTCOUNT);EnvPutInt(NAME_MAXREDIRECTCOUNT,2); \
  int DOENV_x4 = EnvGetLong(NAME_RECONNECTWAIT);EnvPutInt(NAME_RECONNECTWAIT,1); \
  int DOENV_x5 = EnvGetLong(NAME_FIRSTCONNECTMAXCNT);EnvPutInt(NAME_FIRSTCONNECTMAXCNT,1); \
  int DOENV_x6 = EnvGetLong(NAME_READAHEADSIZE);EnvPutInt(NAME_READAHEADSIZE,0); \
  int DOENV_x7 = EnvGetLong(NAME_READCACHESIZE);EnvPutInt(NAME_READCACHESIZE,0);  
//  int DOENV_x8 = EnvGetLong(NAME_DATASERVERCONN_TTL);EnvPutInt(NAME_DATASERVERCONN_TTL, 10);
#define UNDOENV \
  EnvPutInt(NAME_CONNECTTIMEOUT,DOENV_x1); \
  EnvPutInt(NAME_REQUESTTIMEOUT,DOENV_x2); \
  EnvPutInt(NAME_MAXREDIRECTCOUNT,DOENV_x3); \
  EnvPutInt(NAME_RECONNECTWAIT,DOENV_x4); \
  EnvPutInt(NAME_FIRSTCONNECTMAXCNT,DOENV_x5); \
  EnvPutInt(NAME_READAHEADSIZE,DOENV_x6); \
  EnvPutInt(NAME_READCACHESIZE,DOENV_x7); 
//  EnvPutInt(NAME_DATASERVERCONN_TTL,DOENV_x8);


#define DEBUG_TAG "GCLIENT"
#include "debugmacros.h"

const char *gclient::version = VERSION; ///< Client version string


struct timeval t1,t2,t3,t4,t5,t6,t7,t8,t9,t10;
struct timezone tz;
#define take_time(x) gettimeofday(&(x),&(tz))
#define interval(y,x) ((((y).tv_sec-(x).tv_sec)*1000000) + \
                       ( (y).tv_usec-(x).tv_usec))

// for debugging of Error conditions this macro prints out
//     where the error was raised
#define SetErrorType(ARG) DEBUGMSG(8, << __FUNCTION__ << "(" \
            << __LINE__ << ") sets error " << ARG << "\n"); \
        SetErrorType(ARG);

char* _rettime() {
  static char ptime[4096];
  time_t now = time(NULL);
  ctime_r(&now,ptime);
  ptime[strlen(ptime)-1]=0;
  return ptime;
}



struct ns__token {
  int port;
  int session_id;
  char *session_passwd;
  char *session_token;
  char *session_user;
  unsigned int session_expiretime;
  // Note: the nonce is sent in a string containing the bytes as
  // space separated integers
  char *nonce;
};


/** gclient
@param storeToken if set to true the returned connection token will be written
to file, i.e. other gclient applications can make use of it without separately
authenticating.
*/
gclient::gclient(bool storeToken): fConnected(false),streamIndex(0), columnIndex(0), fieldIndex(0),
				  charIndex(0), endOfFile(true) {
  fStoreToken=storeToken;

  // initialize random generator
  srandom(time(NULL));

  if (storeToken)
    fConnected=true;

  codec = new TBytestreamCODEC();
  fdecoutputstream=NULL;
  fauthMode = 0;
  fcq = new SPC_CIPHERQ;
  fEncryptReply=0;
  fRemoteDebug="0";

  if (getenv("DEBUG_GCLIENT")) {
    EnvPutInt(NAME_DEBUG,atoi(getenv("DEBUG_GCLIENT")));
  }
}

gclient::~gclient() {
  delete codec;
  delete fcq;
}

/**
   Connect to an authentication port to obtain a session and a token
   with the needed information. 

   A list of servers can be specified with the environment variable
   GCLIENT_SERVER_LIST. The format is:
   GCLIENT_SERVER_LIST=<host1:port1>|<host2:port2>|<host3:port3>
   It can be used in combination with the GCLIENT_USER environment variable.

   @param host     hostname of server. If given NULL, the value of the
                   environment variable GCLIENT_SERVER_NAME is used.
   @param sslport  authentication port. If <= 0 the value of the
                   environment variable GCLIENT_SERVER_PORT is used
   @param user     user name (or desired role on the grid). If given NULL,
                   the value of the environment variable GCLIENT_USER
                   is used.

   @return returns true if connect succeeded
*/
bool gclient::Connect(const char *host,
		      int port, const char *user, const char *password) {
  char serverlist[4096];
  char* serverurl[16]; // max 16 servers in a server list
  int nserver=0;

  memset(serverurl,0,sizeof(serverurl));

  if (password) {
    fPassword = password;
  } else {
    fPassword = "";
  }

  if (getenv("GCLIENT_SERVER_LIST")) {
      sprintf(serverlist,"%s",getenv("GCLIENT_SERVER_LIST"));
      if (!strlen(serverlist)) {
	fConnected=false;
	fprintf(stderr, "No servers configured in GCLIENT_SERVER_LIST - cannot connect - aborting this application!\n"); 
	SetErrorType("client:connect:no_emptyserverlist");
	exit(-1);
      }
      char* sptr=serverlist;
      char* tok=0;
      while ((tok=strtok(sptr,"|"))) {
	  if (sptr)
	      sptr=0;
	  DEBUGMSG(3,<< "Adding Server " << tok << " to list of servers\n");
	  serverurl[nserver] = tok;
	  nserver++;
      }
  } else {
      if((host==NULL) || (strlen(host)==0)) {
	  if(getenv("GCLIENT_SERVER_NAME")==NULL) {
	      SetErrorType("client:connect:no_host");
	      return(false);
	  }
	  host=getenv("GCLIENT_SERVER_NAME");
	  if ( (host==NULL) || (strlen(host)==0)) {
	    SetErrorType("client:connect:no_host");
	    return(false);
	  }
      }
      if(port<=0) {
	  if(getenv("GCLIENT_SERVER_PORT")==NULL) {
	      SetErrorType("client:connect:no_port");
	      return(false);
	  }
	  port=atoi(getenv("GCLIENT_SERVER_PORT"));
      }
      if(user==NULL) {
	  if(getenv("GCLIENT_USER")==NULL) {
	      SetErrorType("client:connect:no_user");
	      return(false);
	  }
	  user=getenv("GCLIENT_USER");
      }
      nserver++;
  }

  
  random();
  int cyclserv=1+(int) (1.0*nserver*random()/(RAND_MAX+1.0));
  DEBUGMSG(3,<< "Selected Server " << cyclserv << " \n");
  for (int serv=0; serv<nserver;serv++)  {
      // try to connect to one of given gservers in a cycle, starting with a random server
      cyclserv++;
      cyclserv%=nserver;

      fConnected=false;

      if ( (serverurl[0]==0) && (nserver==1) ) {
	  fPort=port;
	  fHost=host;
	  fUser=user;
      } else {
	  string surl = serverurl[cyclserv];
	  string::size_type pos = surl.find(":",0);
	  string::size_type length = surl.length();
	  fHost= surl.substr(0,pos);
	  fPort= atoi((surl.substr(pos+1,length-pos-1)).c_str());	
	  if (user==NULL) {
	      if(getenv("GCLIENT_USER")==NULL) {
		  SetErrorType("client:connect:no_user");
		  return(false);
	      } 
	      fUser=getenv("GCLIENT_USER");
	  } else {
	      fUser=user;
	  }
      }
      fauthMode=AUTH_GSI;

      unsetenv("XrdSecNoSSL");
      SetURL();
      // here we do a special hook to support automatic configuration
      if (getenv("X509_CERT_DIR")) {
	setenv("XrdSecSSLCADIR",getenv("X509_CERT_DIR"),1);
      } else {
	if ((! getenv("XrdSecSSLCADIR")) && (getenv("ROOTSYS"))) {
	  static bool notshown = true;
	  std::string cadir = getenv("ROOTSYS");
	  cadir += "/share/certificates";
	  setenv("XrdSecSSLCADIR",cadir.c_str(),1);
	  if (notshown) {
	  cout << "=> autoconfigure ca-dir to " << cadir.c_str() << endl;
	  notshown = false;
	  }
	}
      }

      cout << "=> Trying to connect to Server [" << cyclserv << "] " << fURL << " as User " << fUser<<" \n";

   
      if (getenv("GCLIENT_NOGSI")) {
	// prevent GSI authentication
	fauthMode=AUTH_NONE;
      }

      if ((getenv("ALIEN_PROC_ID")) && (getenv("ALIEN_JOB_TOKEN")) && (!getenv("GCLIENT_NOTOKEN"))){
	// set sss authentication
	fauthMode=AUTH_JOBTOKEN;
	setenv("XrdSecNoSSL","1",1);
      }
  
      if (fauthMode==AUTH_NONE) {
	SetErrorDetail("No authentication method can be tried!");
	continue;
      }

	
      bool authenticated=false;
      if (fauthMode== AUTH_GSI) {
	DEBUGMSG(5, << "trying authentication: GSI"
		 << "\n");
	
	if(GSIgetToken()==0) {
	  DEBUGMSG(5,<< "GSI authentication ok\n");
	  authenticated=true;
	} else {
	  DEBUGMSG(5,<< "GSI authentication failed\n");
	}
      }
      
      if(fauthMode == AUTH_JOBTOKEN) {
	fauthMode=AUTH_JOBTOKEN;
	if(SSLgetToken()==0) {
	  authenticated=true;
	  DEBUGMSG(5,<< "jobtoken authentication ok\n");
	} 
      }
	
      if (authenticated) {
	if(fStoreToken) {
	  WriteToken(Tokenfilename());
	}
	fConnected=true;
	return true;
      }
  }
  return false;
}
  
/**
   Reconnects to the service stored in the token (necessary if the
   server closed the session due to exceeded idle time)

   @return true if reconnect succeeded
 */
bool gclient::Reconnect() {
  fConnected=false;
  if (strlen(fHost.c_str()) == 0) {
    fprintf(stderr,"Error [%s] GCLIENT::Reconnect : connection host undefined - no reconnection possible [Terminating...]\n",_rettime());
    return false;
  }

  fprintf(stderr,"Warning [%s] GCLIENT::Reconnect\n",_rettime());
  if(fauthMode==AUTH_JOBTOKEN) {
    if(SSLgetToken()<0) {
      SetErrorType("client:reconnect:SSL");
      return false;
    }
  }
  
  if(fauthMode==AUTH_GSI) {
    // remove in any case
    unsetenv("XrdSecNoSSL");
    if(GSIgetToken()<0) {
      SetErrorType("client:reconnect:GSI");
      return(false);
    }
  }

  SetURL();

  if(fStoreToken) {
    WriteToken(Tokenfilename());
  }

  fConnected=true;
  return true;
}


/**
   establishes an SSL context (not yet) with the server and gets the token.

   @return -1 in case of error, 0 for success
 */
int gclient::SSLgetToken(const char* password) {
  DEBUGMSG(5,<< "initiating job token authentication\n");
  string newUser = "jobtoken:" + string(getenv("ALIEN_PROC_ID"));
  fUser = newUser;

  try {
    std::string fNonceString;
    
    DEBUGMSG(5,<< "authentication url=" << GetAuthenURL().c_str() << "\n");
    
    DEBUGMSG(5,<< "Calling JobToken get token\n");
    
    char value[4096]; value[0] = 0;;
    XrdOucString request;
    request = GetAuthenURL().c_str();
    request += "?";
    request += "xalien.user="; request += fUser.c_str();
    request += "&xalien.jobtoken="; request += getenv("ALIEN_JOB_TOKEN");
    request += "&xalien.cmd=getToken";

    DEBUGMSG(5,<< "Request URL is " << request.c_str());

    DOENV;
    XrdClientAdmin admin(request.c_str());
    admin.Connect();
    XrdOucString str(request.c_str());
    XrdClientUrlInfo url(str);
    long long response = admin.Query(kXR_Qopaquf, (const kXR_char*) url.File.c_str(),(kXR_char*)value,4096);
    UNDOENV;

    if (response>0) {
      XrdOucEnv respenv(value);
    
      if (respenv.Get("xalien.session_user")) {
	fUser = respenv.Get("xalien.session_user");
      } else {
	fUser ="nobody";
      }
      if (respenv.Get("xalien.session_passwd")) {
	fTokenPassword = respenv.Get("xalien.session_passwd");
      } else {
	fTokenPassword = "-";
      }
      if (respenv.Get("xalien.session_id")) {
	fTokenSID = atoi(respenv.Get("xalien.session_id"));
      } else {
	fTokenSID = -1;
      }
      if (respenv.Get("xalien.session_expiretime")) {
	fExpiretime = strtol(respenv.Get("xalien.session_expiretime"),0,0);
      } else {
	fExpiretime = 0;
      }
      if (respenv.Get("xalien.nonce")) {
	fNonceString = respenv.Get("xalien.nonce");
      } else {
	fNonceString = "";	
      }
    } else {
      SetErrorType("server:authentication:SSL:gettoken");
      struct ServerResponseBody_Error *e;
      if (( e = admin.LastServerError()) ) {
	SetErrorDetail(e->errmsg);
      } else {
	SetErrorDetail("unknown");
      }
      return (-1);
    }
    
    if (!fNonceString.length()) {
      SetErrorType("server:authentication:SSL:gettoken");
      SetErrorDetail("server didn't reply with a nonce");
      return (-1);
    }
    
    DEBUGMSG(8, << "returned authentication token: port=" << fPort << "\n"
	     << "           session_passwd=" << fTokenPassword << "\n"
	     << "           session_id=" << fTokenSID << "\n");
    
    
    TSharedAuthentication::StringToNonce((char*)fNonceString.c_str(),fNonce);
    fauthMode = AUTH_JOBTOKEN;
  } catch (...) {
    DEBUGMSG(5,<< "catched exception - recovering\n");
    return (-1);
  }

  return(0);
}

/**
   establishes a GSI context with the server and gets a token.

   @return -1 in case of error, 0 for success
 */
int gclient::GSIgetToken() {
  //  static bool firstcall=true;

  try {
    std::string fNonceString;
    
    DEBUGMSG(5,<< "authentication url=" << GetAuthenURL().c_str() << "\n");
    
    DEBUGMSG(5,<< "Calling GSI get token\n");
    
    char value[4096]; value[0] = 0;;
    XrdOucString request;
    request = GetAuthenURL().c_str();
    request += "?";
    request += "xalien.user="; request += fUser.c_str();
    request += "&xalien.cmd=getToken";
    
    
    DOENV;
    XrdClientAdmin admin(request.c_str());
    //    if (!firstcall) {
      // force disconnection if already connected
    //      if (admin.GetClientConn()) {
    //	admin.Connect();
    //	admin.GetClientConn()->Disconnect(true);
    //      }
    //    } else {
    //      firstcall=false;
    //    } 
    admin.Connect();

    XrdOucString str(request.c_str());
    XrdClientUrlInfo url(str);
    long long response = admin.Query(kXR_Qopaquf, (const kXR_char*) url.File.c_str(),(kXR_char*)value,4096);
    UNDOENV;

    if (response>0) {
      XrdOucEnv respenv(value);
    
      if (respenv.Get("xalien.session_user")) {
	fUser = respenv.Get("xalien.session_user");
      } else {
	fUser ="nobody";
      }
      if (respenv.Get("xalien.session_passwd")) {
	fTokenPassword = respenv.Get("xalien.session_passwd");
      } else {
	fTokenPassword = "-";
      }
      if (respenv.Get("xalien.session_id")) {
	fTokenSID = atoi(respenv.Get("xalien.session_id"));
      } else {
	fTokenSID = -1;
      }
      if (respenv.Get("xalien.session_expiretime")) {
	fExpiretime = strtol(respenv.Get("xalien.session_expiretime"),0,0);
      } else {
	fExpiretime = 0;
      }
      if (respenv.Get("xalien.nonce")) {
	fNonceString = respenv.Get("xalien.nonce");
      } else {
	fNonceString = "";
      }
    } else {
      SetErrorType("server:authentication:GSI:gettoken");
      struct ServerResponseBody_Error *e;
      if (( e = admin.LastServerError()) ) {
	SetErrorDetail(e->errmsg);
      } else {
	SetErrorDetail("unknown");
      }
      return (-1);
    }
    
    if (!fNonceString.length()) {
      SetErrorType("server:authentication:GSI:gettoken");
      SetErrorDetail("server didn't reply with a nonce");
      return (-1);
    }
    
    DEBUGMSG(8, << "returned authentication token: port=" << fPort << "\n"
	     << "           session_passwd=" << fTokenPassword << "\n"
	     << "           session_id=" << fTokenSID << "\n");
    
    
    TSharedAuthentication::StringToNonce((char*)fNonceString.c_str(),fNonce);
    fauthMode = AUTH_GSI;
  } catch (...) {
    DEBUGMSG(5,<< "catched exception - recovering\n");
    return (-1);
  }

  return(0);
}





/**
   tests connection with the main communication port of the server

   @return 0 for error, 1 for success
 */
int gclient::ping(void) {

  if(!fConnected) {
          DEBUGMSG(1,<< "not connected\n");
	  SetErrorType("client:not_connected");
          return(0);
  }
  // otherwise 0;
  return(1);
}

/**
   returns connection state
   @return true for connected, false for disconnected
 */
bool gclient::Connected(void) {
  return fConnected;
}

/**
   Gapi shell implementation. If GNU Readline is available, shell history
   and searching is available.
 */
void gclient::Shell() {
  bool flag_dumpresult=false;

  if(!fConnected) {
    SetErrorType("client:not_connected");
    cerr << "you must connect first!\n";
    return;
  }

  Command("cd"); // for setting cwd
  
  while(1) {
    stringstream promptstr;
    promptstr << fUser << "@" << fHost << ":" << fAuthenPort 
	      << " " << cwd() << " >";
    string prompt=promptstr.str();

    char *cmdline = readline(prompt.c_str());
    //printf("p:%p  >%s<\n",cmdline,cmdline);

    if(! cmdline || ! *cmdline) {
      free(cmdline);
      continue;
    }


#ifdef HAVE_READLINE
    add_history(cmdline);
#endif

    if(!strcmp(cmdline,"quit") || !strcmp(cmdline,"exit")) {
      free(cmdline);
      break;
    }
    if(!strcmp(cmdline,"gclient_dump")) {
      flag_dumpresult=!flag_dumpresult;
      cout << "gclient dump mode: " << flag_dumpresult << "\n";
      continue;
    }

    if(!Command(cmdline)) {
      t_gerror gerror = GetErrorType();
      if(ErrorMatches(gerror,"server:authentication")) {
	cerr << "authentication failed; reconnect to the service!\n";
	return;
      }
      if(ErrorMatches(gerror,"server:session_expired")) {
	DEBUGMSG(1,<< "session expired, reconnecting...\n");
	if(!Reconnect()) {
	  DEBUGMSG(1, << "unable to reconnect to " << GetAuthenURL().c_str() << "\n");
	  exit(-1);
	}
      }
    }
    
    
    if(flag_dumpresult) {
      DebugDumpStreams();
    } else {
      PrintCommandStderr();
      PrintCommandStdout();
    }

    free(cmdline);
  }

}


#ifndef HAVE_READLINE

/**
   Replacement function if GNU readline is unavailable.
 */
char *gclient::readline(const char *prompt) {
  char cmdline[65535], *ptr, *result;

  DEBUGMSG(5,<< "custom readline invoked (GNU readline missing on this system)\n");

  ptr=cmdline;
  printf("%s",prompt);
  while ((*ptr=getchar())) {
    if ((ptr-cmdline)>=65534) {
      fprintf(stderr,"Error: exceeded maximum commandline length!\n");
      ptr = cmdline;
      break;
    }
    if(*ptr=='\n') {
      *ptr='\0';
      break;
    }

    ptr++;
  }

  result = (char*) malloc(strlen(cmdline)+1);
  strcpy(result,cmdline);
  return(result);
}
#endif


/**
   Sends a Grid command for resolution at the server via the main
   communication port.
   In case of a persistent token (file based) the token is read and
   stored again after the connection (part of the token changes with
   every information exchange to forestall packet replay attacks).
   The command and results are encoded via the TBytestreamCodec class.
   All necessary encryption/decryption is done via the TSharedAuthentication
   class.

   @param command the command in form of a C string

   @return false in case of an error condition. Information about the
           error is available through member functions.
 */

bool gclient::Command(const char *command) {
  int maxtry = 1; 
  int ntry  = 0;
  bool success;
  int serverreselect=0;
  int serverreconnect=0;
  int retrysleep = 5;
  float dampingfactor= 1.2;
  time_t maxwait = 0;
  static int lasttimeexecuted=0;

  // use environment settings
  if (getenv("GCLIENT_COMMAND_RETRY")) {
    maxtry = atoi(getenv("GCLIENT_COMMAND_RETRY"));
  }
  if (getenv("GCLIENT_COMMAND_MAXWAIT")) {
    maxwait = atoi(getenv("GCLIENT_COMMAND_MAXWAIT"));
  }
  if (getenv("GCLIENT_SERVER_RESELECT")) {
    serverreselect = atoi(getenv("GCLIENT_SERVER_RESELECT"));
  }
  if (getenv("GCLIENT_SERVER_RECONNECT")) {
    serverreconnect = atoi(getenv("GCLIENT_SERVER_RECONNECT"));
  }
  if (getenv("GCLIENT_RETRY_SLEEPTIME")) {
    retrysleep = atoi(getenv("GCLIENT_RETRY_SLEEPTIME"));
  }
  if (getenv("GCLIENT_RETRY_DAMPING")) {
    dampingfactor = atof("getenv(GCLIENT_RETRY_DAMPING");
  }

  // rescale to usefule values
  if (retrysleep <=0) {
    retrysleep = 5;
  }
  if (retrysleep > 300) {
    retrysleep = 300;
  }
  if (maxwait <=0) {
    maxwait = 3600*24; // more than a day makes no sense
  }
  if (dampingfactor<1) {
    dampingfactor=1.2;
  }
  if (dampingfactor > 2.0) {
    dampingfactor=2.0;
  }


  int tryselect=0;
  int tryconnect=0;
  time_t startwait=time(NULL);


  //  if ( fConnected && lasttimeexecuted && ((startwait - lasttimeexecuted + 1 ) > EnvGetLong(NAME_DATASERVERCONN_TTL))) {
    // we have to reconnect if the TTL has expired
  //    if (fauthMode=AUTH_GSI) {
  //      Reconnect();
  //    }
  //  }

  lasttimeexecuted = startwait;

  try {
    success = InternalCommand(command);
  } catch ( ... ) {
    success = false;
  }

  if (!success) {
    Reconnect();

    for (;;) {
      try {
	success=InternalCommand(command);
	if (success) return success;
      } catch ( ... ) {
	success = false;
      }
      ntry++;
      tryselect++;
      tryconnect++;
      if ( ( time(NULL) - startwait) > maxwait) {
	fprintf(stderr,"Warning [%s] GCLIENT::COMMAND command failed %d times - waittime %u exhausted - giving up.\n",_rettime(),ntry,(unsigned int)maxwait);
	// timeout due to maximal retry wait time
	return success;
      }
      if ( ( ntry >= maxtry) ) {
	fprintf(stderr,"Warning [%s] GCLIENT::COMMAND command failed %d times - max retry %u reached - giving up.\n",_rettime(),ntry,maxtry);
	// timeout due to maximal retry count
	return success;
      }
      if ( tryconnect >= serverreconnect ) {
	tryconnect = 0;
	fprintf(stderr,"Warning [%s] GCLIENT::COMMAND try to reconnect to the same service after %d failures \n",_rettime(),serverreconnect);
	Reconnect();
	continue;
      }
      if ( tryselect >= serverreselect ) {
	tryselect = 0;
	fprintf(stderr,"Warning [%s] GCLIENT::COMMAND try to establish a new connection after %d failures \n",_rettime(),serverreselect);
	if (fPassword.length()) {
	  Connect(0, 0, fUser.c_str(), fPassword.c_str());
	} else {
	  Connect(0, 0, fUser.c_str(), 0);
	}
	continue;
      }
      fprintf(stderr,"Warning [%s] GCLIENT::COMMAND retry %u of %u - timeout in %ld seconds - sleeping %d seconds \n",_rettime(),ntry, maxtry, (maxwait - (time(NULL)-startwait)), retrysleep);
      sleep(retrysleep);
      retrysleep = (int) (retrysleep*1.0*dampingfactor);
    }
  }

  return success;
}


bool gclient::InternalCommand(const char *command) {
  take_time(t1);
  // lock and read the token file
  string filename=Tokenfilename();
  ClearError();

  string gridcwd;
  string envFilename;
  if (fStoreToken) {
      envFilename=gbboxEnvFilename();
  }
  ifstream envFile(envFilename.c_str());
  if(envFile && (fStoreToken)) {
    char line[255];
    envFile.getline(line,255);
    gridcwd = line;
    envFile.close();
    DEBUGMSG(4, << "Read CWD from" << envFilename
             << ": >" << gridcwd << "<\n");
    setCWD(gridcwd);
  }
  take_time(t2);
  int fd;

  DEBUGMSG(5, << "fStoreToken is " << fStoreToken << "\n");
  if(fStoreToken) {
    if((fd=open(filename.c_str(),O_RDWR|O_CREAT,S_IWUSR|S_IRUSR))==-1) {
      cerr << "failed to open " << filename << "\n";
      SetErrorType("client:tokenfile:read");
      return(false);
    }
    if(flock(fd,LOCK_EX)!=0) {
      cerr << "WARNING: failed to lock tokenfile\n";
    }
    if(ReadToken(filename)==-1)
      fConnected=false;
  }

  if(!fConnected) {
    if (fStoreToken) {
      if(flock(fd,LOCK_UN)!=0) {
	cerr << "WARNING: failed to unlock gclient tokenfile\n";
      }
      close(fd);
    }
    SetErrorType("client:not_connected");
    return(false);
  }
  take_time(t3);
  //////////////////////
  size_t len=strlen(command);
  char *buf = new char [len+1];
  strcpy(buf,command);
  
  int nofcommands=1;
  for(unsigned int i=0;i<len;i++) {
    if(isspace(buf[i]))
      nofcommands++;
  }
  
  char **list = new char*[nofcommands+1];

  bool endoftoken=true;
  nofcommands=0;
  char* argptr=NULL;
  for(unsigned int i=0;i<(len+1);i++) {
    if (nofcommands==1) {
      if (getenv("GCLIENT_EXTRA_ARG") && strlen(getenv("GCLIENT_EXTRA_ARG"))) {
	list[1] = getenv("GCLIENT_EXTRA_ARG");
	nofcommands++;
      }
    }

    if ((endoftoken) && (!isspace(buf[i])) && (buf[i])) {
      endoftoken = false;
      argptr = buf+i;
    }
    
    if ( (!endoftoken) && ((isspace(buf[i]) || (!buf[i]) ) ) ){
      buf[i]='\0';
      list[nofcommands] = argptr;
      nofcommands++;
      endoftoken=true;
    }
  }
  //////////////////////
  take_time(t4);
  // just for debugging
  //for(int i=0;i<nofcommands;i++) {
  //  DEBUGMSG(8,<< "list[" << i << "]: >" << list[i] << "<\n");
  //}
  char *codeccommand=codec->EncodeInputArray(nofcommands,list);
  DEBUGMSG(8,<< "codeccommand: >" << codeccommand << "<\n");
  delete [] list;
  delete [] buf;

  take_time(t5);
  // prepare the hash that is used to pass
  // additional information to the perl module
  std::map<std::string, std::string> statehash;
  statehash["pwd"]=fcurrentDir;

  if(fEncryptReply)
    statehash["encryptReply"]="y";
  statehash["remoteDebug"]=fRemoteDebug.c_str();

  stringstream codecstring;
  DEBUGMSG(8,<< codec->EncodeInputHash(statehash) << "n");

  codecstring << codeccommand << (char) codec->kOutputterminator
	      << codec->EncodeInputHash(statehash) << (char) codec->kStreamEnd;
  take_time(t6);

  if (codeccommand)
    delete [] codeccommand;

  size_t keylen;
  int b64err;
  unsigned char* tmp_key =
    spc_base64_decode((unsigned char *)fTokenPassword.c_str(),&keylen,1,&b64err);
  DEBUGMSG(7,<< "b64 enc key: >"<< fTokenPassword 
	   <<"<   decoded key length " << keylen <<"\n");
  spc_cipherq_setup(fcq,tmp_key,SPC_DEFAULT_KEYSIZE,1);
  spc_cipherq_setnonce(fcq,fNonce);

  if (tmp_key)
    free(tmp_key);

  ////////////////////////////
  // Encription of the client's authentication magic token
  DEBUGMSG(5,<< "ENCRYPTING MAGIC TOKEN\n");
  char *magic_token= new char[strlen(AUTHMAGIC)+1];
  //  char magic_uutoken[1024];
  //char magic_crypto[1024];

  strcpy(magic_token,AUTHMAGIC);

  char *magic_crypto= (char*)
    spc_cipherq_b64encrypt(fcq,(unsigned char *)magic_token,
			   strlen(magic_token));
  if (magic_token)
    delete [] magic_token;
  DEBUGMSG(10,<< "ecrypted magic token: >" << magic_crypto
	   << "< (" << strlen(magic_crypto) << ")\n");


  char *inputstream= (char*)
    spc_cipherq_b64encrypt(fcq,(unsigned char *)codecstring.str().c_str(),
						 codecstring.str().size());

  take_time(t7);

  char* outputstream=0;
  char sessionid[1024];
  sprintf(sessionid,"%d",fTokenSID);
  std::string* soutputstream = new std::string;
  // call the xrootd server

  std::string request="";
  request = GetAuthenURL().c_str();
  request += "?";
  request += "&xalien.session_id=";
  request += sessionid;
  request += "&xalien.cmd=execute";
  request += "&xalien.magic=";
  request += magic_crypto;
  free(magic_crypto);
  request += "&xalien.inputstream=";
  request += inputstream;
  DEBUGMSG(8,<< "Encrypted command:>" << inputstream
	   << "<    length:" << strlen(inputstream) << "\n");
  //  soapexec::soap_call_ns__stream(soap,fURL.c_str(), "", inputstream, &outputstream);
  bool failed=false;
  
  // we set short timeouts for everything here
  
  DOENV;

  // we avoid ssl authentication

  XrdClient* xclient = new XrdClient(request.c_str());
  
  UNDOENV;

  if (xclient && xclient->Open(0,false) && (xclient->LastServerResp()->status == kXR_ok ) ) {
    static char inbuffer[(1024*512)+1];
    ssize_t bSize = 1024*512;
    off_t  bOffset = 0;
    long nread=0;
    // synchronous open
    do {
      nread = xclient->Read(inbuffer,bOffset, bSize); 
      if (nread >=0) {
	inbuffer[nread+1] = 0;
	*soutputstream += inbuffer;
	bOffset+= nread;
      }
    } while (nread == bSize);
    *soutputstream += '\0';
    // delete  afterwards
    delete xclient;
  } else {
    // error
    char errorline[1024];
    struct ServerResponseBody_Error *e;
    SetErrorType("client:result");
    if ( xclient && (e = xclient->LastServerError()) ) {
      sprintf(errorline,"retc=%d\tmsg='%s'\n" ,e->errnum ,e->errmsg);
      SetErrorDetail(errorline);
    } else {
      SetErrorDetail("failed to create XrdClient object");
    }
    failed=true;
  }

  free(inputstream);
  take_time(t8);

  //  printf("Server returned %s\n", soutputstream.c_str());

  DEBUGMSG(1,<< "Timing: t2 " << interval(t2,t1) << " microsec\n");
  DEBUGMSG(1,<< "Timing: t3 " << interval(t3,t2) << " microsec\n");
  DEBUGMSG(1,<< "Timing: t4 " << interval(t4,t3) << " microsec\n");
  DEBUGMSG(1,<< "Timing: t5 " << interval(t5,t4) << " microsec\n");
  DEBUGMSG(1,<< "Timing: t6 " << interval(t6,t5) << " microsec\n");
  DEBUGMSG(1,<< "Timing: t7 " << interval(t7,t6) << " microsec\n");
  DEBUGMSG(1,<< "Timing: t8 " << interval(t8,t7) << " microsec\n");
  DEBUGMSG(1,<< "Timing: t0 " << interval(t8,t1) << " microsec\n");

  if (failed) {
    if(fStoreToken) {
      if(flock(fd,LOCK_UN)!=0) {
	fprintf(stderr,"WARNING: failed to unlock tokenfile\n");
      }
      close(fd);
    }
    return(false);
  }
  ////////////////////////////

  std::string returntoken;
  std::string server_magic_token;

  // assign server magic & outputstream 
  int pos = soutputstream->find("xalien.outputstream=");
  

  if (pos != STR_NPOS) {
    outputstream = (char*)(soutputstream->c_str()+pos + strlen("xalien.outputstream="));
    returntoken = soutputstream->substr(0,pos);
  } else {
    outputstream = NULL;
    returntoken = *soutputstream;
  }

  //  printf("returntoken = %s\n", returntoken.c_str());
  XrdOucEnv env(returntoken.c_str());
  
  if (env.Get("xalien.magic")) {
    server_magic_token=env.Get("xalien.magic");
  } else {
    cerr << "Error: Cannot find the magic token!\n";
    SetErrorType("client:authentication:magic_token:missing");

    if(fStoreToken) {
      WriteToken(filename);
      if(flock(fd,LOCK_UN)!=0) {
	fprintf(stderr,"WARNING: failed to unlock tokenfile\n");
      }
      close(fd);
    }
    return(false);
  }

  ////////////////////////////
  // Decription and testing of the server's authentication magic token
  DEBUGMSG(5, << "DECRYPTING SERVER'S MAGIC TOKEN: "
	   << server_magic_token.c_str() << "\n");

  size_t cryptlen;
  magic_token=(char *)
    spc_cipherq_b64decrypt(fcq,(unsigned char*) server_magic_token.c_str(),
			   &cryptlen);
  if(!magic_token) {
    cerr << "Error: Cannot decrypt the magic token!\n";
    SetErrorType("client:authentication:magic_token:decrypt");
    if(fStoreToken) {
      WriteToken(filename);
      if(flock(fd,LOCK_UN)!=0) {
	fprintf(stderr,"WARNING: failed to unlock tokenfile\n");
      }
      close(fd);
    }
    return(false);
   }

  DEBUGMSG(5,<< "magic token is: " << magic_token <<"\n");

  if(strcmp(magic_token,AUTHMAGIC_CL)) {
    SetErrorType("client:authentication:magic_token:no_match");
    free(magic_token);
    if(fStoreToken) {
      WriteToken(filename);
      if(flock(fd,LOCK_UN)!=0) {
	fprintf(stderr,"WARNING: failed to unlock tokenfile\n");
      }
      close(fd);
    }
    return(false);
  }
  ////////////////////////////

  free(magic_token);

  //////////////////////////////////////////////////////
  // Decryption of the server's result string
  if(outputstream==NULL) {
    SetErrorType("client:result");
    memcpy(fNonce,spc_cipherq_getnonce(fcq),SHMAUTH_NONCE_SIZE);
    if(fStoreToken) {
      WriteToken(filename);
      if(flock(fd,LOCK_UN)!=0) {
	fprintf(stderr,"WARNING: failed to unlock tokenfile\n");
      }
      close(fd);
    }
    
    return(false);
  } 
  if(fdecoutputstream) free(fdecoutputstream);

  if(fEncryptReply) {
    DEBUGMSG(5,<< "DECRYPTING SERVER'S RESULT STRING\n");
    DEBUGMSG(7,<< "result string: >" << outputstream << "<\n");
    
    fdecoutputstream=(char *)
      spc_cipherq_b64decrypt(fcq,(unsigned char*) outputstream,
			     &cryptlen);

    if (outputstream) {
      // free the encoded stream here
      delete soutputstream;
    }

    if(!fdecoutputstream) {
      SetErrorType("client:result:decrypt");
      
      memcpy(fNonce,spc_cipherq_getnonce(fcq),SHMAUTH_NONCE_SIZE);
      if(fStoreToken) {
	WriteToken(filename);
	if(flock(fd,LOCK_UN)!=0) {
	  fprintf(stderr,"WARNING: failed to unlock tokenfile\n");
	}
	close(fd);
	return(false);
      }
    } 
  } else {
    // that is not really perfect
    fdecoutputstream=strdup(outputstream);
    outputstream=NULL;
    delete soutputstream;
  }

  int check=codec->Decode(fdecoutputstream,strlen(fdecoutputstream));
  DEBUGMSG(5,<< "codec decoding returned status=" << check << "\n");

  // get env hash and set environment
  std::map<std::string,std::string> envhash;
  for(int i=0;i<codec->GetStreamField(codec->kENVIR,0,
				    TBytestreamCODEC::kFieldname);i++) {
    //DEBUGMSG(10, << "Filling env hash\n"
    //     << codec->GetFieldName(codec->kENVIR,0,i)
    //     << " => "<< codec->GetFieldValue(codec->kENVIR,0,i) << "\n");
    envhash[codec->GetFieldName(codec->kENVIR,0,i)]=
	    codec->GetFieldValue(codec->kENVIR,0,i);
  }

  if(envhash.find("error")!=envhash.end()) {
    SetErrorType(envhash["error"].c_str());
    if(envhash.find("errortxt")!=envhash.end())
      SetErrorDetail(envhash["errortxt"].c_str());
    if(fStoreToken) {
      if(flock(fd,LOCK_UN)!=0) {
	fprintf(stderr,"WARNING: failed to unlock tokenfile\n");
      }
      close(fd);
    }
    return(false);
  }

  if(envhash.find("pwd")!=envhash.end()) {
    fcurrentDir=envhash["pwd"].c_str();
    DEBUGMSG(8,<< "client PWD set to >" << envhash["pwd"] << "<\n");
  }
  
  //if(check) codec->DumpResult();
  
  // Reset indices for the streamed output
  endOfFile = false;
  streamIndex = columnIndex = fieldIndex = charIndex=0;


  memcpy(fNonce,spc_cipherq_getnonce(fcq),SHMAUTH_NONCE_SIZE);
  TSharedAuthentication::IncrementNonce(fNonce);
  if(fStoreToken) {
    WriteToken(filename);
    if(flock(fd,LOCK_UN)!=0) {
      fprintf(stderr,"WARNING: failed to unlock tokenfile\n");
    }
    close(fd);
  }

  DEBUGMSG(8,<< "returning from command >" << command << "<\n");
  return check;
}

/** Reads a string from the response of the server. Whitespace is
    skipped at the beginning and the string is read until whitespace
    occurs. The response is handed back via @str.

    This function is part of the stream interface to the server
    response and allows you to parse it like a C++ cin stream.
*/
gclient &gclient::operator>> (string &str)
{

  str = "";
  skipWhitespace();

  while(streamIndex <= TBytestreamCODEC::kOUTPUT) {
    while (columnIndex < codec->GetStreamColumns(streamIndex)) {
      while (fieldIndex < codec->GetStreamField(streamIndex,columnIndex,
						  TBytestreamCODEC::kFieldvalue)) {
	const char *field;
	field=codec->GetFieldValue(streamIndex,columnIndex,fieldIndex)+charIndex;
	int len = strcspn(field, "\t ");
	if(len){
	  str.append(field, len);
	  charIndex+=len;
	  return *this;
	}
	fieldIndex++;
	charIndex = 0;
      }
      columnIndex++;
      fieldIndex = 0;
    }
    streamIndex++;
    columnIndex = 0;
  }
  return *this;
}


/** Fills the key-value pairs of a response from the server into the
    the associative array @tags. @column is the column of the
    response you are interested in. The tags are extracted from stream
    2 of the response.
    The return value is the number of tags found.
*/
unsigned int 
gclient::readTags(int column, std::map<std::string, std::string> &tags) const
{
  int stream = 2;
  tags.clear();
  if(column<0 || column >= codec->GetStreamColumns(stream))
    return 0;

  for(int field=0;
      field < codec->GetStreamField(stream,column, TBytestreamCODEC::kFieldvalue);
      field++)
  {
    tags[codec->GetFieldName(stream,column,field)]
      =codec->GetFieldValue(stream,column,field);
  }
  return tags.size();
}

/** Returns the number of columns in the result stream.
    @param stream index of the result stream
    @return -1 result stream does not exist
*/
int 
gclient::GetStreamColumns(int stream) const {
  return (codec>0)?codec->GetStreamColumns(stream):-1;
}

/** Returns the number of rows in the result stream for a column.
    @param stream index of the result stream
    @param column index of the column to return
    @return -1 result stream does not exist
*/
int 
gclient::GetStreamRows(int stream, int column) const {
  return (codec>0)?codec->GetStreamField(stream,column, TBytestreamCODEC::kFieldvalue):-1;
}

/** Returns the a field value in stream specified by row and column
    @param stream index of the result stream
    @param column index of the column 
    @param field index of the field
    @return -1 result stream does not exist
*/
char* 
gclient::GetStreamFieldValue(int stream, int row, int column) {
  return (codec>0)?(char*)codec->GetFieldValue(stream,row,column):NULL;
}

char*
gclient::GetStreamFieldKey(int stream, int row, int column) {
  return (codec>0)?(char*)codec->GetFieldName(stream,row,column):NULL;
}

/**
   returns a string containing information about the connected server
   caller responsible for freeing the string via delete
   
   @return    information string
*/
const string gclient::getServerInfo()
{
  if(fStoreToken)
    if(ReadToken(Tokenfilename())==-1)
      return "";
  
  std::stringstream str;
  str << "gapiOverXroot" ;
  //std::string *res = new std::string;
  
  return str.str();
}



/**
   Returns the command's result as a newly allocated t_result
   (a vector of <string,string> hashes)
   The invoking function must care for deallocation of the result!
   Since the extensive use of STL containers may make this routine
   quite slow, it should only be used where appropriate.

   @param (out) res the results in a t_result structure
   @return number of results
*/
int gclient::getResult(t_gresult &res) const
{
  int nres=codec->GetStreamColumns(codec->kOUTPUT);
  res.reserve(nres);
  for(int i=0;i<nres;i++) {
    t_gresultEntry dummy;
    readTags(i,dummy);
    res.push_back(dummy);
  }
  return nres;
}

/** Reads a full line from the response of the server. Whitespace is
    not skipped. The result is returned in via the @str reference.

    This function is part of the stream interface to the server
    response and allows you to parse it like a C++ cin stream.
*/
gclient &gclient::getline(std::string &str)
{
  str="";

  if(streamIndex > TBytestreamCODEC::kOUTPUT){
    endOfFile = true;
    return *this;
  }
    

  if(columnIndex < codec->GetStreamColumns(streamIndex)){
    while(fieldIndex 
	  < codec->GetStreamField(streamIndex, columnIndex,
				  TBytestreamCODEC::kFieldvalue))
      {
	str.append(codec->GetFieldValue(streamIndex,
					columnIndex,
					fieldIndex)+charIndex);
	fieldIndex++;
      }
    fieldIndex=charIndex=0;
    columnIndex++;
  } else {
    streamIndex++;
    columnIndex = fieldIndex = charIndex = 0;
    str="\n";
  }
  endOfFile = true;
  return *this;
}

/** Reads an unsigned long value the response of the server. Whitespace is
    skipped at the beginning and the string is read until whitespace
    occurs. The response is handed back via the reference @i:
    \code
    unsigned int i;
    gclient >> i;
    \endcode

    This function is part of the stream interface to the server
    response and allows you to parse it like a C++ cin stream.
*/
gclient &gclient::operator>> (unsigned long long &i)
{
  char *ptr;

  while(streamIndex <= TBytestreamCODEC::kOUTPUT) {
    while (columnIndex < codec->GetStreamColumns(streamIndex)) {
      while (fieldIndex < codec->GetStreamField(streamIndex,columnIndex,
						TBytestreamCODEC::kFieldvalue)){
	const char *field;
	field = codec->GetFieldValue(streamIndex,columnIndex,fieldIndex)
	        + charIndex;
	i=strtoll(field, &ptr, 10);
	charIndex+=(ptr-field);
	if(ptr!=field)
	  return *this;
	fieldIndex++;
	charIndex = 0;
      }
      columnIndex++;
      fieldIndex = 0;
    }
    streamIndex++;
    columnIndex = 0;
  }
  endOfFile = true;
  return *this;
}


/** Reads @len characters from the response of the server. Whitespace
    is not skipped at the beginning. The response is handed back via @str.

    This function is part of the stream interface to the server
    response and allows you to parse it like a C++ cin stream.
*/
gclient &gclient::readsome(string &str, int len){
  str = "";
  
  while(streamIndex <= TBytestreamCODEC::kOUTPUT) {
    while (columnIndex < codec->GetStreamColumns(streamIndex)) {
      while (fieldIndex < codec->GetStreamField(streamIndex,columnIndex,
						TBytestreamCODEC::kFieldvalue)){
	const char *field;
	field = codec->GetFieldValue(streamIndex,columnIndex,fieldIndex)
                + charIndex;
	int oldSize=str.size();
	str.append(field, len-oldSize);
	charIndex+=(str.size()-oldSize);
	if((int)str.size()>=len)
	  return *this;
	fieldIndex++;
	charIndex = 0;
      }
      columnIndex++;
      fieldIndex = 0;
    }
    streamIndex++;
    columnIndex = 0;
  }
  endOfFile = true;
  return *this;
}


/** Skip Whitespace in the gclient input stream.

    This function is part of the stream interface to the server
    response and allows you to parse it like a C++ cin stream.
*/
gclient &gclient::skipWhitespace()
{
  while(streamIndex <= TBytestreamCODEC::kOUTPUT) {
    while (columnIndex < codec->GetStreamColumns(streamIndex)) {
      while (fieldIndex < codec->GetStreamField(streamIndex,columnIndex,
						TBytestreamCODEC::kFieldvalue)){
	const char *field;
	field = codec->GetFieldValue(streamIndex,columnIndex,fieldIndex)
	        + charIndex;
	while(*field){
	  if(!isspace(*field)) return *this;
	  field++;
	  charIndex++;
	}
	fieldIndex++;
	charIndex = 0;
      }
      columnIndex++;
      fieldIndex = 0;
    }
    streamIndex++;
    columnIndex = 0;
  }
  endOfFile = true;
  return *this;
}

int gclient::PrintCommandStdout() const {
  int streamid=codec->kSTDOUT;
  int xmax = codec->GetStreamColumns(streamid);
  for (int x=0; x < xmax; x++) {
    for (int y=0; y <
	   codec->GetStreamField(streamid,x,codec->kFieldvalue);y++) {
      std::cout << codec->GetFieldValue(streamid,x,y);
    }
  }
  return(1);
}
int gclient::PrintCommandStderr() const {
  int streamid=codec->kSTDERR;
  int xmax = codec->GetStreamColumns(streamid);
  for (int x=0; x < xmax; x++) {
    for (int y=0; y <
	   codec->GetStreamField(streamid,x,codec->kFieldvalue);y++) {
      std::cerr << codec->GetFieldValue(streamid,x,y);
    }
  }
  return(1);
}
int gclient::DebugDumpStreams() const {
  codec->DumpResult();
  return(1);
}

const char* gclient::GetMotd() {
  fMotd="";
  if (Command("motd")) {
    for (int i=0; i< GetStreamColumns(0);i++) {
      const char* line = GetStreamFieldValue(0,i,0);
      if (line) {
	fMotd += std::string(line);
      }
    }
    return fMotd.c_str();
  } else {
    return 0;
  }
}


/**
  Used to obtain the filename for the session token

  @return token filename
 */
const string gclient::Tokenfilename() {
  stringstream strm;

  if(getenv("GCLIENT_TOKENFILE")!=NULL)
    strm << getenv("GCLIENT_TOKENFILE");
  else
    strm << "/tmp/" << "gclient_token_" << getuid();
  return strm.str();
}

// TODO: currently writing/saving of session tokens is done in clear text with
// the only protection afforded by file access restrictions. Might have to
// be improved.
// TODO: think about how to implement possibility for multiple sessions
// with different contact points
/**
   Writes the session token to file

   @param filename file to save token to
 */
int gclient::WriteToken(const string filename) {
  FILE *fh;
  int check;

  DEBUGMSG(8, << "writing token to " << filename << "\n");

  if((fh=fopen(filename.c_str(),"w")) == NULL) {
    cerr << "failed to open token file " << filename << "\n";
    return(-1);
  }

  if ((check=chmod(filename.c_str(), S_IRUSR| S_IWUSR))) {
    cerr << "failed to set safe permission on the token file " << filename << "\n";
    return(-1);
  }

  fprintf(fh,"Host = %s\n",fHost.c_str());
  fprintf(fh,"Port = %d\n",fAuthenPort);
  fprintf(fh,"Port2 = %d\n",fPort);
  fprintf(fh,"User = %s\n",fUser.c_str());
  fprintf(fh,"Passwd = %s\n",fTokenPassword.c_str());
  fprintf(fh,"Nonce = ");
  char *noncestr = TSharedAuthentication::NonceToString(fNonce);
  fprintf(fh," %s\n",noncestr);
  delete[] noncestr;
  fprintf(fh,"SID = %d\n",fTokenSID);
  fprintf(fh,"mode = %d\n",fauthMode);
  fprintf(fh,"encryptReply = %d\n",fEncryptReply);
  fprintf(fh,"Expiretime = %d\n",fExpiretime);
  fprintf(fh,"remoteDebug = %s\n",fRemoteDebug.c_str());
  if ((check=fclose(fh))==EOF) {
    cerr << "failed to close token file " << filename << "\n";
    return(-1);
  }
  return(0);
}


/**
   Read the token from a file

   @param filename file to read token from
 */
int gclient::ReadToken(const string filename) {
  FILE *fh;
  const int entries=12;
  char line[entries][LINELENGTH+1],strdummy[LINELENGTH+1];
  int check=0;

  DEBUGMSG(8,<< "reading token from " << filename << "\n");

  //  umask(066);
  if( (fh=fopen(filename.c_str(),"r")) == NULL) {
    cerr << "failed to open token file " << filename << "\n";
    return(-1);
  }
  
  for(int i=1; i<entries; i++) {
    if(fgets(line[i],LINELENGTH,fh) == NULL) {
      DEBUGMSG(5,<< "failed reading token file line\n");
      return(-1);
    }
    //DEBUGMSG(5,<<"tokenfile: " << line[i] << "\n");
  }
    
  if((check=sscanf(line[1]," Host = %s",strdummy))) {
    fHost = strdummy;
  } else return(-1);

  if((check=sscanf(line[2]," Port = %d",&fAuthenPort))!=1) return(-1);
  if((check=sscanf(line[3]," Port2 = %d",&fPort))!=1) return(-1);

  if((check=sscanf(line[4]," User = %s",strdummy))) {
    fUser = strdummy;
  } else return(-1);

  if((check=sscanf(line[5]," Passwd = %s",strdummy))) {
    fTokenPassword = strdummy;
  } else return(-1);


  if(strstr(line[6],"Nonce =")) {
    char *strptr;
    strptr=line[6]+strlen("Nonce =");
    if(TSharedAuthentication::StringToNonce(strptr,fNonce)==0) {
      DEBUGMSG(5,<< "read Nonce = " << strptr << "\n");
    }
  } else return(-1);

  if((check=sscanf(line[7]," SID = %d",&fTokenSID))!=1) return(-1);

  if((check=sscanf(line[8]," mode = %d",&fauthMode))!=1) return(-1);

  if((check=sscanf(line[9]," encryptReply = %d",&fEncryptReply))!=1) return(-1);
  if((check=sscanf(line[10]," Expiretime = %d",&fExpiretime))!=1) return(-1);  
  if((check=sscanf(line[11]," remoteDebug = %s",strdummy))) {
    fRemoteDebug = strdummy;
  } else  return(-1);

  if (fclose(fh)==EOF) {
    cerr << "failed to close token file " << filename << "\n";
    return(-1);
  }

  DEBUGMSG(8,<< "Read token file\n");

  SetURL();

//   // test connection via ping
//   soap->header->ns__sid = fTokenSID;
//   soap->header->ns__crypto = NULL;

//   //  printf("Address %d\n",soap->header);
//   soapexec::soap_call_ns__ping(soap, fURL.c_str(), "", NULL, result);
//   if (soap->error) {
//     SetErrorType("client:not_connected:ping");
//     SetErrorDetail(soap->fault->faultstring);
//     return(-1);
//   }
//   DEBUGMSG(10,<< "ping result (" << fURL << ") = " << result << "\n");

  fConnected=true;
  return(0);
}


const string gclient::GetAuthenURL() {
  stringstream d;

  switch(fauthMode) {
  case AUTH_JOBTOKEN:
    d << "root://";
    break;

  case AUTH_GSI:
    d << "root://";
    break;
  default:
    std::stringstream msg;
    msg << "client:authentication:unknown_mode:" << fauthMode;
    SetErrorType(msg.str().c_str());
    return("");
  }

  d << fHost << ":" << fPort << "//dummy";
  return d.str();
}


/**
   Sets URL for the main connection based on the member variables
   for host and port.
 */
void gclient::SetURL() {
  stringstream h;
  if(fPort>99999) {
    cerr << "wrong port number: " << fPort << "\n";
    fPort=8000;
    cerr << "port set to " << fPort << "\n";
  }
  h << "root://" << fHost << ":" << fPort ;
  fURL=h.str();
}

/**
   Allows setting options specific to this GapiUI implementation
   
   @param key     name of the option
   @param value   value
   @return    0 :ok, 1: problem with option name, 2: problem with value
*/
int gclient::setOption(std::string key, std::string value) {
  DEBUGMSG(8, << "setOption key: " << key << " value: " << value << "\n");
  if(key=="encrypt") {
    setResultEncryption(true);
    return(0);
  } else if (key=="noencrypt") {
    setResultEncryption(false);
    return(0);
  } else if (key=="remotedebug") {
    setRemoteDebug(value);
    return(0);
  }
  
  return(1);
}

/**
   sets the option indicating that the server replies should contain
   encrypted results

   &param b      true for enable encryption
*/
void gclient::setResultEncryption(bool b) {
  string filename=Tokenfilename();
  int fd;
  if(fStoreToken) {
    if((fd=open(filename.c_str(),O_RDWR|O_CREAT,S_IWUSR|S_IRUSR))==-1) {
      cerr << "failed to open " << filename << "\n";
      SetErrorType("client:tokenfile:read");
      exit(-1);
    }
    if(flock(fd,LOCK_EX)!=0) {
      cerr << "WARNING: failed to lock tokenfile\n";
    }
    if(ReadToken(filename)==-1) {
      cerr << "No token!\n";
      exit(-1);
    }
  }

  fEncryptReply=b;

  if(fStoreToken) {
    WriteToken(filename);
    if(flock(fd,LOCK_UN)!=0) {
      fprintf(stderr,"WARNING: failed to unlock tokenfile\n");
    }
    close(fd);
  }
}

/**
   sets the option indicating that the server commands should be 
   run in debug mode

   &param b      true for enable encryption
*/
void gclient::setRemoteDebug(std::string b) {
  string filename=Tokenfilename();
  int fd;
  if(fStoreToken) {
    if((fd=open(filename.c_str(),O_RDWR|O_CREAT,S_IWUSR|S_IRUSR))==-1) {
      cerr << "failed to open " << filename << "\n";
      SetErrorType("client:tokenfile:read");
      exit(-1);
    }
    if(flock(fd,LOCK_EX)!=0) {
      cerr << "WARNING: failed to lock tokenfile\n";
    }
    if(ReadToken(filename)==-1) {
      cerr << "No token!\n";
      exit(-1);
    }
  }

  fRemoteDebug=b;

  if(fStoreToken) {
    WriteToken(filename);
    if(flock(fd,LOCK_UN)!=0) {
      fprintf(stderr,"WARNING: failed to unlock tokenfile\n");
    }
    close(fd);
  }
}

/**
   checks whether an error is of a certain type

   @param error the returned error to be compared
   @param type type description of the error

   @return true if error is of the given type
*/
bool gclient::ErrorMatches(const t_gerror &error, const std::string &type) const {
  if(! error.compare(0,type.length(),type)) return true;
  return false;
}

