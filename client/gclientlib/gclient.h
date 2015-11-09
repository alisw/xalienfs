#ifndef __gclient_h_included
#define __gclient_h_included
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <string>
#include <map>
#include <vector>

#include "../config.h"

#include "gapiUI.h"


#include "../ShmAuth/ShmAuth.h"


class TBytestreamCODEC;

#define AUTH_NONE 0
#define AUTH_PASS 1
#define AUTH_GSI 2
#define AUTH_JOBTOKEN 3

#define LINELENGTH 200

using namespace std;


/** @class gclient gclient.h
 
     Implementation class of the GapiUI virtual class.
  
     @author D. Feichtinger
     @author A. Peters
     @date 06/08/2004
 */
class gclient: public GapiUI {
 protected:
  bool  fConnected; ///< state flag indicating whether we are connected
  string  fHost; ///< host runnning the mtpoolserver to which we are connected
  int   fAuthenPort; ///< port to which we authenticate
  int   fPort;///< main communication port
  string fPassword; ///< normally the token used for the initial conenection
  string  fUser; ///< GRID user login name
  string fURL;    ///< XROOT server URL for main communications
  string fTokenPassword; ///< session password for an authenticated session (b64 encoded key)
  string fMotd;   ///< MOTD, if the motd has once been called

  unsigned char fNonce[SHMAUTH_NONCE_SIZE]; ///< nonce used for encryption (+ to counter replay attacks)
  SPC_CIPHERQ *fcq;  ///< cipher queue object used for encryption/decription
  int   fTokenSID;      ///< session ID for an authenticated session
  int   fauthMode;  ///< authentication mode used (PAM or GSI)
  bool fStoreToken;  ///< flag indicating whether a persistent token (true)
                     /// is wanted

  std::string fcurrentDir; ///< current Grid working directory
  int fEncryptReply; ///< flag indicating whether server should encrypt results
  std::string fRemoteDebug ; ///< debug tags indicating whether server should run commands in debug mode
  unsigned int fExpiretime; ///< unix timestamp indicating the expire time of this token
  t_gerror fErrorType; ///< contains last error
  string fErrorDetail; ///< human readable error description

  TBytestreamCODEC *codec; ///< codec used to (de)serialize the result structures
  char *fdecoutputstream; ///< codec-decoded output stream buffer
  static const char *version; ///< client's version string

#ifndef HAVE_READLINE
  char *readline(const char *prompt);
#endif
  void setResultEncryption(bool b);
  void setRemoteDebug(std::string b);


 public:
  gclient(bool storeToken=true);
  ~gclient();
  bool Command(const char* command);
  bool InternalCommand(const char* command);
  bool Connect(const char *host, int sslport, const char *user, const char *password=0);
  bool Connected(void);
  const std::string GetAuthenURL();
  const string GetSSLURL(); //TODO: this should be GetAuthenURL()
  const string GetGSIURL();
  int ping(void);
  bool Reconnect();
  void Shell();

  gclient &operator>> (std::string &str);
  gclient &operator>> (unsigned long long &i);
  bool eof() const { return endOfFile; };
  gclient &readsome(std::string &s, int len);
  gclient &getline(std::string &s);
  gclient &skipWhitespace();
  unsigned int readTags(int column, std::map<std::string, std::string> &tags) const;
  int GetStreamRows(int streamIndex, int column) const;
  int GetStreamColumns(int stream) const;
  char* GetStreamFieldValue(int stream, int row, int column);
  char* GetStreamFieldKey(int stream, int row, int column);
  const char* GetUser() { return fUser.c_str();}
  int getResult(t_gresult &res) const;
  int PrintCommandStdout() const;
  int PrintCommandStderr() const;
  int DebugDumpStreams() const;
  const char* GetMotd();

  /**
     returns client's current Grid working directory

     @return client's Grid working directory
  */
  const string &cwd() const { return fcurrentDir; };

  /**
     This lets clients set the current working directory.
     The information is only used in the next Command()
     executed on the server, i.e. whether the value makes
     sense is not checked as part of this function.
     @param path new current directory path
  */
  void setCWD(string path) {
    fcurrentDir=path;
  }
  const string getServerInfo();
  const char *getVersion() {
    return(version);
  }

  int setOption(std::string key, std::string value);

  const t_gerror GetErrorType() { return fErrorType; }
  const string GetErrorDetail() { return fErrorDetail; }
  bool ErrorMatches(const t_gerror &error, const std::string &type) const;

 private:
  int streamIndex; ///< current stream index (stream interface)
  int columnIndex; ///< current column index (stream interface)
  int fieldIndex; ///< current column index (stream interface)
  int charIndex; ///< current column index (stream interface)
  bool endOfFile; ///< end of file state (stream interface)


/**
   Sets a string used to identify the last error.
   E.g. "server:authentication:GSI:context". The colon is used
   to establish a hierarchical structure for errors.

   @param msg string containing error identification
*/
  void SetErrorType(const char *msg) { if (msg) fErrorType=msg; }

/**
   Sets a string describing details of the last error
   (human readable)
*/
  void SetErrorDetail(const char *msg) { if (msg) fErrorDetail=msg; }

/**
   Clears the member variables identifying the last error
 */
  void ClearError() { fErrorType=fErrorDetail="none"; }
;

  void SetSSLURL();
  void SetGSIURL();
  void SetURL();
  int SSLgetToken(const char* password=0);
  int GSIgetToken();
  int ReadToken(const string filename);
  int WriteToken(const string filename);
  const std::string Tokenfilename();
};

#endif
