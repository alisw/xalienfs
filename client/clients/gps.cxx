#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include "gapiUI.h"
#include "gapi_job_operations.h"
#include "config.h"

#define DEBUG_TAG "GPS"

#include "debugmacros.h"

#define PROGNAME "ps"

using namespace std;

// Note: GPREFIX (the grid command prefix) is defined in configure.in
void usage() {
      fprintf (stderr, "\n");
      fprintf (stderr, "usage: %s \t -F {l} (output format)\n",PROGNAME);
      fprintf (stderr, "          \t -f <flags/status>\n");
      fprintf (stderr, "          \t -u <userlist>\n");
      fprintf (stderr, "          \t -s <sitelist>\n");
      fprintf (stderr, "          \t -n <nodelist>\n");
      fprintf (stderr, "          \t -m <masterjoblist>\n");
      fprintf (stderr, "          \t -o <sortkey>\n");
      fprintf (stderr, "          \t -j <jobidlist>\n");
      fprintf (stderr, "          \t -l <query-limit>\n");
      fprintf (stderr, "          \t -q <sql query>\n");
      fprintf (stderr, "          \n");
      fprintf (stderr, "          \t -M show only masterjobs\n");
      fprintf (stderr, "          \t -X active jobs in extended format\n");
      fprintf (stderr, "          \t -A select all owned jobs of you\n");
      fprintf (stderr, "          \t -W select all jobs which are waiting for execution of you\n");
      fprintf (stderr, "          \t -E select all jobs which are in error state of you\n");
      fprintf (stderr, "          \t -a select jobs of all users\n");
      fprintf (stderr, "          \t -b do only black-white output\n");
      fprintf (stderr, "          \t -jdl   <jobid>                          : display the job jdl\n");
      fprintf (stderr, "          \t -trace <jobid> [trace-tag[,trace-targ]] : display the job trace information\n");
}

int main(int argc, char* argv[]) {
  //Usage: ps2 <flags> <status> <users> <sites> <nodes> <masterjobs> <order> <jobid> <limit> <sql>
  string print_format("");

  string st_flags("-");
  string st_users("-");
  string st_sites("-");
  string st_nodes("-");
  string st_masterjobs("-");
  string st_order("-");
  string st_jobid("-");
  string st_limit("-");
  string st_sql("-");
  int flag;
  int skipoptions=0;
  bool colourout=true;
  if (argc>=2) {
    if (!strcmp(argv[1],"-jdl")) {
      if (argc<3) {
	usage();
	exit(-1);
      } else {
	// produce the command like -jdl <queueid> ....
	print_format="J";
	skipoptions=1;
	st_flags=std::string(argv[1]);
	st_users="";
	for (int loop=2; loop<argc;loop++) {
	  st_users.append(" ");
	  st_users.append(std::string(argv[loop]));
	}

	st_sites="";
	st_nodes="";
	st_masterjobs="";
	st_order="";
	st_jobid="";
	st_limit="";
	st_sql="";
      }
    }
    if (!strcmp(argv[1],"-trace")) {
      if (argc<3) {
	usage();
	exit(-1);
      } else {
	// produce the command like -trace <queueid> <trace-flags...>
	print_format="T";
	skipoptions=1;
	st_flags=std::string(argv[1]);
	st_users="";
	for (int loop=2; loop<argc;loop++) {
	  st_users.append(" ");
	  st_users.append(std::string(argv[loop]));
	}

	st_sites="";
	st_nodes="";
	st_masterjobs="";
	st_order="";
	st_jobid="";
	st_limit="";
	st_sql="";
      }
    }
  }

  if (!skipoptions) {
    while ((flag = getopt (argc, argv, "hbWEAaXMF:f:u:t:u:s:n:m:o:j:l:q:")) != -1) {
      switch(flag) {
      case 'F':
	print_format = optarg;
	continue;
      case 'f':
	st_flags = optarg;
	continue;
      case 'u':
	st_users = optarg;
	continue;
      case 's':
	st_sites = optarg;
	continue;
      case 'n':
	st_nodes = optarg;
      case 'm':
	st_masterjobs = optarg;
	continue;
      case 'o':
	st_order = optarg;
	continue;
      case 'j':
	st_jobid = optarg;
	continue;
      case 'l':
	st_limit = optarg;
      continue;
      case 'q':
	st_sql = optarg;
	continue;
      case 'X':
	print_format = "l";
	st_flags = "-rs";
	continue;
      case 'A':
	st_flags = "-a";
	continue;
      case 'a':
	st_users = "%";
	continue;
      case 'E':
	st_flags = "-f";
	continue;
      case 'W':
	st_flags = "-s";
	continue;
      case 'M':
	st_masterjobs = "\\\\0";
	continue;
      case 'b':
	colourout=false;
	continue;
      default:
	usage();
	exit(-1);
      } 
    }
  }
  

  string bname = basename(argv[0]);
  
  GAPI_JOBARRAY* gjobarray = gapi_queryjobs(st_flags,st_users,st_sites,st_nodes,st_masterjobs,st_order,st_jobid,st_limit,st_sql);
  if (getenv("alien_NOCOLOR_TERMINAL")) {
    colourout=false;
  } 
  gapi_printjobs(gjobarray,print_format,colourout);  
  exit(0);
}
