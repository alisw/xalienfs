#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "config.h"

int main(int argc, char* argv[]) {
  char nogridpath[4096];
  sprintf(nogridpath,"%s_NOGRID_PATH",GPREFIX);
  setenv("PATH",getenv(nogridpath),1);

  //  printf("Setting Path to %s\n",getenv(nogridpath));
  if (argc <2) {
    // run a normal bash
    return execlp("bash","-l","-i",NULL,NULL);

  } else {
    const char* s[65535];
    char joinedstring[65535];
    joinedstring[0] = 0;
    s[0] = "-l";
    s[1] = "-i";
    s[2] = "-c";
    for (int i=1; i< argc; i++ ) {
      if ( (strlen(joinedstring) + strlen(argv[i])) < 65500) {
	strcat(joinedstring,argv[i]);
	strcat(joinedstring," ");
      } else {
	fprintf(stderr,"Error: argument line too long\n");
	return -1;
      }
    }

    s[3] = joinedstring;
    s[4] = NULL;
    return execvp("bash",(char *const* )s);
  }
}
