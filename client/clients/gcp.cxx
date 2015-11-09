#include "gapi_file_operations.h"

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024*1024

#define PROGNAME "cp"

void usage() {
  fprintf (stderr, "\n");
  fprintf (stderr, "usage: %s \t <src> <dest> \n",PROGNAME);
  fprintf (stderr, "          \t Return Values:\n");
}


int main(int argc, char *argv[])
{
  int i;
  int result;
  if (argc>2) {
    if (!strcmp(argv[1],"-c")) {
      char systemline[4096];
      sprintf(systemline,"gbbox cp ");
      for (i=2;i < argc; i++) {
	strcat(systemline,argv[i]);
	strcat(systemline," ");
      }
      printf("Executing: %s\n",systemline);
      return system(systemline);
    }
  }

  result =  gapi_copy(argc, argv, BUFFER_SIZE);
  switch (result) {
  case -99 :
    usage();
    exit(1);
  case -10:
    fprintf(stderr,"Error: cannot open source file %s\n",argv[1]);
    exit(2);
  case -11:
    fprintf(stderr,"Error: cannot open destination file %s\n",argv[2]);
    exit(3);
  case -13:
    fprintf(stderr,"Error: cannot allocate the copy buffer of %d bytes\n",BUFFER_SIZE);
    exit(1);
  case -20:
    fprintf(stderr,"Error: write operation failed on the destination file\n");
    exit(4);
  }
  exit(1);
}
