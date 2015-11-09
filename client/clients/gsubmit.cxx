#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include "gapiUI.h"
#include "gapi_job_operations.h"
#include "config.h"

#define DEBUG_TAG "GSUBMIT"

#include "debugmacros.h"

#define PROGNAME "submit"

using namespace std;

string gs_textblack("\033[49;30m");
string gs_textred("\033[49;31m");
string gs_textrederror("\033[47;31m");
string gs_textblueerror("\033[47;31m");
string gs_textgreen("\033[49;32m");
string gs_textyellow("\033[49;33m");
string gs_textblue("\033[49;34m");
string gs_textbold("\033[1m");
string gs_textunbold("\033[0m");

void print_help() {
  fprintf (stderr, "\n");
  fprintf (stderr, "usage: %s \t <URL> \n",PROGNAME);
  fprintf (stderr, "          \t <URL> => alien://<lfn>\n");
  fprintf (stderr, "          \t <URL> => /alien<lfn>\n");
  fprintf (stderr, "          \t <URL> => <local path>\n");
  fprintf (stderr, "          \t -h \n");
}  

// Note: GPREFIX (the grid command prefix) is defined in configure.in

int main(int argc, char* argv[]) {
  string print_format("");

  int flag;
 
  if (argc == 1 ) {
    print_help();
    exit(-1);
  }

  while ((flag = getopt (argc, argv, "h")) != -1) {
    switch(flag) {
    case 'h':
    default:
      print_help();
      exit(-1);
    } 
  }
  
  
  string jdlurl = (argv[1]);
  string args = "";
  bool colourout=true;
  for (int narg = 2; narg < argc; narg++) {
    args += " ";
    args += string(argv[narg]);
  }

  if (getenv("alien_NOCOLOR_TERMINAL")) {
    colourout=false;
  }

  GAPI_JOBID jobid = gapi_submit(jdlurl.c_str(),args.c_str());  
  if (jobid == 0) { 
    if (colourout) {
      fprintf(stderr, "submit: %s%sError submitting job-jdl %s%s%s\n",gs_textbold.c_str(), gs_textred.c_str(),jdlurl.c_str(),gs_textblack.c_str(),gs_textunbold.c_str());
    } else {
      fprintf(stderr, "submit: Error submitting job-jdl %s\n",jdlurl.c_str());
    }
    exit(-1);
  } else {
    if (colourout) {
      fprintf(stdout,"submit: Your new job ID is %s%s%d%s%s \n",gs_textbold.c_str(),gs_textblue.c_str(),(int)jobid,gs_textunbold.c_str(),gs_textblack.c_str());
    } else {
      fprintf(stdout,"submit: Your new job ID is %d \n",(int)jobid);
    }
    exit(0);
  }
}
