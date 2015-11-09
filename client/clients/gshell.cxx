#include <stdio.h>
#include <config.h>
#include "gapiUI.h"





int main(int argc, const char* argv[]) {
  GapiUI *gc = GapiUI::MakeGapiUI(false);
  if (argc != 1 && argc != 4 ) {
    fprintf(stderr,"usage: gshell [<host> <sslport> <user>]\n");
    exit(-1);
  }
  const char *host,*user;
  int port;
  if(argc == 1) {
    host=NULL;
    port=-1;
    user=NULL;
  } else {
    host=argv[1];
    port=atoi(argv[2]);
    user=argv[3];
  }
  gc->Connect(host,port,user);

  if (!gc) {
    fprintf(stderr,"Cannot connect!\n");
    delete gc;
    exit(-1);
  } else {
    gc->Shell();
  }

  delete gc;
  exit(0);
}
