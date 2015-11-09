#ifndef __SHMAUTH_H
#define __SHMAUTH_H
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
//#include <openssl/evp.h>

#include "../cwccrypt/spc_cypherq.h"

#define STDIN           0
#define STDOUT          1
#define BUFLEN          512
#define MAPDIR          "/tmp/"

//#define ENCRYPT         1
//#define DECRYPT         0
//#define ENCODE          1
//#define DECODE          0

/////////////////////////////////////////////////////////////////////////
// UU ENCODING
// ENC is the basic 1 character encoding function to make a char printing 



//#define ALG             EVP_des_ede3_cbc()
//#define ALG             EVP_aes_128_ecb()
//#define ALG   EVP_aes_128_cbc()
#define SHMAUTH_NONCE_SIZE 16
#define SHMAUTH_UID_SIZE 1024
#define AUTHMAGIC "Peters-Feichtinger-Productions"
#define AUTHMAGIC_CL "proudly presents 5 orange Llamas"
#define MAXAUTHENTICATION 10000
#define MINUUBUFFERSIZE 1000000
#define DEFAULTLIFETIME 86400

#ifdef DEBUG
#define DEBUG_TRACE(x) do { printf("DEBUG: %s \t %s\t %s\n",__func__,__PRETTY_FUNCTION__,(x));} while(0)
#else
#define DEBUG_TRACE(x) while (0) {};
#endif


class TSharedAuthentication {
 private:
  int fShmid;
  unsigned int fdefaultlifetime;
  struct StAuthblock {
    unsigned int sid;
    unsigned int exptime;
    unsigned int clockdiff;
    unsigned int token;
    unsigned int actime;
    unsigned int cmdcnt;
    unsigned int ip;
    SPC_CIPHERQ q;
    char uid[SHMAUTH_UID_SIZE];
  };
  unsigned int fToken;
  unsigned int fModToken;
  bool fMaster;
  char fCrypttoken[64];
  char fCryptedtoken[64];
  char fCrypto[128];
  char *fUUbuffer;
  int fUUbuffersize;
  
  struct StAuthblock* fAuthblock;
 public:
  TSharedAuthentication(){TSharedAuthentication(true);};
  TSharedAuthentication(bool master,const char* sharedid="main");
  ~TSharedAuthentication();

  enum {kDecrypt = 0, kEncrypt = 1};


  void Info(const char* txt) {
    fprintf(stdout,"TSharedAuthentication [INFO]: %s",txt);
  }
  void Warn(const char* txt) {
    fprintf(stderr,"TSharedAuthentication [WARN]: %s",txt);
  }
  void Error(const char* txt) {
    fprintf(stderr,"TSharedAuthentication [ERROR]: %s",txt);
  }
  void Fatal(const char* txt) {
    fprintf(stderr,"TSharedAuthentication [FATAL]: %s",txt);
    exit(-1);
  }  

  struct StAuthblock* ReadBlock(unsigned int sid);
  unsigned int Getsid();
  unsigned int GetIp(unsigned int sid);
  void SetIp(unsigned int sid, unsigned int ip);
  void Dumpsids();
  unsigned int GetActiveSessions(); 
  void WriteUserInfoFile(const char* filename);
  unsigned int GetNSessions();


  bool Authorize(unsigned int sid, unsigned int token);

  SPC_CIPHERQ *GetCipherContext(int sid) {
    if(sid<1 || sid>MAXAUTHENTICATION)
      return(NULL);
    return &fAuthblock[sid].q;
  }

  int GetLifetime(int i);
  int GetIdletime(int i);

  const int GetDefaultLifetime() const {
    return fdefaultlifetime;
  }

  void SetDefaultLifetime(unsigned int dltime) {
    fdefaultlifetime=dltime;
  }

  bool SetUser(int sid, char* user) {
    if ( ( sid>0) && (sid<MAXAUTHENTICATION)) {
      if (strlen(user) < SHMAUTH_UID_SIZE) {
        strcpy(fAuthblock[sid].uid,user);
        return true;
      } else {
        Error("User name exceeds maximum allowed length");
        return false;
      }
    }
    return false;
  }

  bool MarkAccess(int sid) {
    if ( ( sid>0) && (sid<MAXAUTHENTICATION)) {
      fAuthblock[sid].actime = time(NULL);
      return true;
    } else {
      Error("Illegal SID - out of bounce");
      return false;
    }
  } 

  const char* GetUser(int sid) {
    if ( ( sid>0) && (sid<MAXAUTHENTICATION)) {
      return fAuthblock[sid].uid;
    } else {
      return NULL;
    }
  }

  unsigned int GetCmdcnt(int sid) {
    if ( ( sid>=0) && (sid<MAXAUTHENTICATION)) {
      return fAuthblock[sid].cmdcnt;
    } else {
      return 0;
    }
  }
  
  void IncrementCmdcnt(int sid) {
    if ( ( sid>0) && (sid<MAXAUTHENTICATION)) {
      fAuthblock[sid].cmdcnt++;
    } 
  }

  void IncrementSessionTotal() {
    fAuthblock[0].sid++;
  }

  void IncrementCommandTotal() {
    fAuthblock[0].cmdcnt++;
  }

  // TODO: (derek) nonce had better be a separate object
  int CreateNonce(unsigned char *nonce);
  //  unsigned char *GetNonce(unsigned int sid);
  static int IncrementNonce(unsigned char *nonce);
  static int PrintNonce(unsigned char *nonce);
  static char *NonceToString(unsigned char *nonce);
  static int StringToNonce(char* str, unsigned char *nonce);

  static int CreatePassword(char* password, int length){
    
    char* ptr= password;
    struct timeval tv;
    struct timezone tz;
    for (int i = 0 ; i < length; i++) {
      gettimeofday(&tv,&tz);
      srand((unsigned int) (tv.tv_usec&0xffffffff));
      *ptr=63+(int) (64.0*rand()/(RAND_MAX+1.0));
      if ( (*ptr) == 0) {
	*ptr = 1;
      }
      ptr++;
    }
    password[length-1]=0;
    return 0;
  }
};


#endif

