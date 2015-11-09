/////////////////////////////////////////////
// gbbox - Grid Busy Box
//         shell command execution tool
// 
// Author: Derek Feichtinger 2004-06-16
//         <derek.feichtinger@cern.ch>
/////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>

#include "gapiUI.h"
#include "config.h"

#define DEBUG_TAG "GBBOX"
#include "debugmacros.h"

using namespace std;

// Note: GPREFIX (the grid command prefix) is defined in configure.in

int main(int argc, char* argv[]) {
  char *cmd=NULL;
  int argstart;
  int dumpresult=0;
  char *order;

  string bname = basename(argv[0]);

  if(!strncmp(bname.c_str(),GPREFIX,strlen(GPREFIX)) &&
     bname.length() > strlen(GPREFIX)) { // called via symlink
    cmd=(char*) &(bname.c_str())[strlen(GPREFIX)+1];
    argstart=1;
  } else if(argc>1) { // we were called as: gbbox [opt] command args
    if(!strcmp(argv[1],"-d") && argc>2) {
      //cout << "dump mode 1\n";
      dumpresult=1;
      cmd=argv[2];
      argstart=3;
    } else if(!strcmp(argv[1],"-o") && argc>3) {
      //cerr << "dump mode 2\n";
      order=argv[2];
      dumpresult=2;
      cmd=argv[3];
      argstart=4;
    } else if(!strcmp(argv[1],"-h")) {
      cout << "NAME: gbbox - Grid Busy Box for executing remote commands\n"
	   << "\nSyntax: gbbox command args - returns stdout/stderr of command\n"
	   << "      gbbox -o f1,f2,... comand args - returns only fields with\n"
	   <<"        tags f1,f2,... "
	   << "      gbbox -d comand args - debug dump output\n"
	   << "If gbbox is called via a symbolic link, it will strip the\n"
	   << "prefix '" << GPREFIX << "' from the command and execute the remaining\n"
	   << "string as command to gbbox\n";
      exit(0);
    }else {
      cmd=argv[1];
      argstart=2;
    }
  } else {
    printf("usage: %s command [arg1 arg2 ...]\n",argv[0]);
    exit(-1);
  }

  int argstrlen=0;
  for(int i=argstart; i<argc;i++) {
    argstrlen+=strlen(argv[i])+1;
  }

  stringstream argstrm;
  argstrm << cmd;
  for(int i=argstart; i<argc;i++)
    argstrm << " " << argv[i];

  string cmdandargs = argstrm.str();

  DEBUGMSG(8,<< "command: " << cmdandargs << "\n")
  ///////////////////////////////////////////////////////////////

  GapiUI *gc = GapiUI::MakeGapiUI();

  string gridcwd;
  string envFilename=gbboxEnvFilename();
  ifstream envFile(envFilename.c_str());
  if(envFile) {
    char line[255];
    envFile.getline(line,255);
    gridcwd = line;
    envFile.close();
    DEBUGMSG(4, << "Read CWD from" << envFilename
	     << ": >" << gridcwd << "<\n");
    gc->setCWD(gridcwd);
  }
  
  if(!strcmp(cmd,"version")) {
    cout << "gclient library version: " << gc->getVersion() << "\n";
    string info=gc->getServerInfo();
    if(info!="")
      cout << "\n--- Server Info: (" << gc->GetAuthenURL() << ") ---\n"
	   << info << "\n";
    exit(0);
  }

  if(!strcmp(cmd,"connect")) {
    if( argc-argstart != 0 && argc-argstart != 3) {
      cerr << "connect syntax: connect host port user      or\n"
	   << "                connect    (without arguments)\n";
      delete gc;
      exit(-1);
    }
    const char *host,*user;
    int port;
    if(argc-argstart == 0) {
      host=NULL;
      port=-1;
      user=NULL;
    } else {
      host=argv[argstart];
      port=atoi(argv[argstart+1]);
      user=argv[argstart+2];
    }
    if(!gc->Connect(host,port,user)) {
      cerr << "failed to connect: " << gc->GetErrorType() << "\n"
	   << "Detail: " << gc->GetErrorDetail() << "\n";
      delete gc;
      exit(-1);
    }
    delete gc;
    exit(0);
  }
  if(!strcmp(cmd,"option")) {
    string key,value;
    int check;
    if(argc-argstart != 1 && argc-argstart != 2) {
      cerr << "syntax: option option_name [value]\n";
      exit(-1);
    }
    key=argv[argstart];
    if(argc-argstart == 1) {
      value="";
    } else {
      value=argv[argstart+1];
    }

    if((check=gc->setOption(key,value))!=0) {
      if(check==1) {
	cerr << "Unknown option: " << key << "\n";
	exit(-1);
      }
      cerr << "Value >" << value << "< not allowed for option " << key << "\n";
      exit(-1);
    }

    exit(0);
  }

  if(!gc->Command(cmdandargs.c_str())) {
    t_gerror gerror = gc->GetErrorType();
    if(gc->ErrorMatches(gerror,"server:authentication") ||
       gc->ErrorMatches(gerror,"server:command:decrypt")) {
      DEBUGMSG(1,<< "authentication failed, reconnecting...\n");
      if(!gc->Reconnect()) {
	cerr << "unable to reconnect to " << gc->GetAuthenURL() <<"\n";
	delete gc;
	exit(-1);
      }
      if (!gc->Command(cmdandargs.c_str())) {
	cerr << "Could not execute command: " << gerror << ": " << gc->GetErrorDetail() << "\n";
	exit(-1);
      }
    } else if(gc->ErrorMatches(gerror,"server:session_expired")) {
      DEBUGMSG(1,<< "session expired, reconnecting...\n");
      if(!gc->Reconnect()) {
	cerr << "unable to reconnect to " << gc->GetAuthenURL() <<"\n";
	delete gc;
	exit(-1);
      }
      if (!gc->Command(cmdandargs.c_str())) {
	cerr << "Could not execute command: " << gerror << ": " << gc->GetErrorDetail() << "\n";
	exit(-1);
      }
    } 
  }

  int exitvalue=0;

  if(dumpresult==1) {
    gc->DebugDumpStreams();
  } else if(dumpresult==2) {


    t_gresult res;
    int nres=gc->getResult(res);
    DEBUGMSG(5,<< "No of results: " << nres << "\n");

    char *vorder[100];
    int nord=0;
    vorder[nord]=strtok(order,",");
    while(vorder[nord]!=NULL && nord<100) {
      nord++;
      vorder[nord]=strtok(NULL,",");
    }

    t_gresultEntry::const_iterator j;
    for(int i=0;i<nres;i++) {
      //for(j=res[i].begin(); j != res[i].end(); j++){
      //	cout << "##" << j->first << "####" << j->second;
      //}
      //cout << "\n";
      for(int k=0;k<nord;k++) {
	if(res[i].find(vorder[k])==res[i].end()) {
	  cerr << "ERROR: no such field tag: " << vorder[k] << "\n";
	  exit(1);
	}
	cout << (res[i])[vorder[k]] << "   ";
      }
      cout << "\n";
    }
  } else {
    gc->PrintCommandStderr();
    gc->PrintCommandStdout();

    t_gresult res;
    int nres=gc->getResult(res);

    if((!nres) || (res[0].find("__result__")==res[0].end())) {
      exitvalue=0;
    } else {
      if (res[0]["__result__"] == std::string("1")) {
	exitvalue=0;
      } else {
	exitvalue=1;
      }
    }
  }

  ofstream envFileOut(envFilename.c_str());
  if(envFileOut) {
    envFileOut << gc->cwd() << endl;
    envFileOut.close();
    DEBUGMSG(4,<< "wrote CWD to " << envFilename
	     << " : >" << gc->cwd() << "<\n");
  }
  

  delete gc;
  exit(exitvalue);
}
