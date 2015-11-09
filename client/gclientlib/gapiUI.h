#ifndef __gapiUI_h_included
#define __gapiUI_h_included

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <iostream>
#include <string>
#include <map>
#include <vector>

#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>

#define DEBUGMACRO 1
 
const std::string gbboxEnvFilename();

// grid result entry
typedef std::map<std::string,std::string> t_gresultEntry;

// grid result
typedef std::vector<t_gresultEntry> t_gresult;

// grid error
typedef std::string t_gerror;

class GapiUI {

 public:
  virtual ~GapiUI() {} ;

  static GapiUI * MakeGapiUI(bool storeToken=true); 

  void SetGapiUI(GapiUI* ui);

  static GapiUI* GetGapiUI();

  // connection related functions
  virtual bool Connect(const char *host, int sslport, const char *user, const char *password=0) = 0;
  virtual bool Connected(void) = 0;
  virtual bool Reconnect() = 0;
  virtual const std::string GetAuthenURL() = 0;

  // command execution and result retrieval/parsing functions
  virtual bool Command(const char* command) = 0;
  virtual int getResult(t_gresult &res) const = 0;
  virtual unsigned int readTags(int column, std::map<std::string, std::string> &tags) const = 0;
  virtual int GetStreamColumns(int stream) const = 0;
  virtual char* GetStreamFieldValue(int stream, int row, int column) = 0;
  virtual char* GetStreamFieldKey(int stream, int row, int column) = 0;
  virtual int GetStreamRows(int stream, int column) const = 0;
  virtual int PrintCommandStdout() const = 0;
  virtual int PrintCommandStderr() const = 0;
  virtual const char* GetMotd() = 0;

  virtual GapiUI &operator>> (std::string &str) = 0;
  virtual GapiUI &operator>> (unsigned long long &i) = 0;
  virtual bool eof() const = 0;
  virtual GapiUI &readsome(std::string &s, int len) = 0;
  virtual GapiUI &getline(std::string &s) = 0;
  virtual GapiUI &skipWhitespace() = 0;

  // miscellaneous
  virtual const std::string &cwd() const = 0;
  virtual const char *getVersion() = 0;
  virtual const std::string getServerInfo() = 0;
  virtual void setCWD(std::string path) = 0;
  virtual const char* GetUser() = 0;
  virtual void Shell() = 0;
  virtual int ping(void) = 0;
  virtual int setOption(std::string key, std::string value) = 0;

  // error handling helpers
  virtual const t_gerror GetErrorType() = 0;
  virtual bool ErrorMatches(const t_gerror &error,
			    const std::string &type) const = 0;
  virtual const std::string GetErrorDetail() = 0;

  virtual int DebugDumpStreams() const = 0;

};

#endif
