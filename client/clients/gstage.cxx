#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include "gapiUI.h"
#include "gapi_file_operations.h"
#include "config.h"

#define DEBUG_TAG "GISONLINE"

#include "debugmacros.h"

#define PROGNAME "stage"

using namespace std;

// Note: GPREFIX (the grid command prefix) is defined in configure.in
void usage() {
      fprintf (stderr, "\n");
      fprintf (stderr, "usage: %s \t <lfn>\n",PROGNAME);
      fprintf (stderr, "          \t Return Values:\n");
      fprintf (stderr, "          \t              : 0  = File is beeing staged");
      fprintf (stderr, "          \t              : -1 = File couln't be staged ");
      fprintf (stderr, "          \t              : -2 = File is not accessible right now");
      fprintf (stderr, "          \t              : -3 = File is not existing\n");
}

int main(int argc, char* argv[]) {
  int result;

  if (argc!=2) {
    usage();
    exit(1);
  } 
  
  if ((result=gapi_prepare(argv[1]))!=0) {
    switch (result) {
    case -1:
      fprintf(stderr,"[%s]: file %s couln't be staged\n",PROGNAME,argv[1]);
      exit (1);
      break;
    case -2:
      fprintf(stderr,"[%s]: file %s is not accessible\n",PROGNAME,argv[1]);
      exit (2);
      break;
    case -3:
      fprintf(stderr,"[%s]: file %s does not exist\n",PROGNAME,argv[1]);
      exit (3);
      break;
    default:
      fprintf(stderr,"[%s]: unknown return value %d\n",PROGNAME,result);
      exit (99);
    }
  } else {
    fprintf(stderr,"[%s]: file %s is beiing staged\n",PROGNAME,argv[1]);
    exit(0);
  }
}
