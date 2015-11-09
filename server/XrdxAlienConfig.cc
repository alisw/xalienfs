//          $Id: XrdxAlienConfig.cc,v 1.16 2007/10/04 01:34:19 ajp Exp $


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <dirent.h>

#include "XrdxAlienFs/XrdxAlienFs.hh"
#include "XrdxAlienFs/XrdxAlienTrace.hh"
#include "XrdOuc/XrdOucStream.hh"
#include "XrdOuc/XrdOucTrace.hh"
#include "XrdSys/XrdSysError.hh"
#include "XrdSys/XrdSysPlugin.hh"

extern XrdOucTrace xAlienFsTrace;

/******************************************************************************/
/*                      L o c a l   V a r i a b l e s                         */
/******************************************************************************/

int XrdxAlienFs::Configure(XrdSysError &Eroute) 
{
  char *var;
  const char *val;
  int  cfgFD, NoGo = 0;

  AlienOutputDir  = "/tmp/";
  AlienPerlModule = "/opt/alien/api/scripts/gapiserver/AlienAS.pl";
  AlienWorker = 10;

  XrdOucStream Config(&Eroute, getenv("XRDINSTANCE"));
  XrdOucString role="server";
  if (getenv("XRDDEBUG")) xAlienFsTrace.What = TRACE_MOST | TRACE_debug;

  if( !ConfigFN || !*ConfigFN) {
    Eroute.Emsg("Config", "Configuration file not specified.");
  } else {
    // Try to open the configuration file.
    //
    if ( (cfgFD = open(ConfigFN, O_RDONLY, 0)) < 0)
      return Eroute.Emsg("Config", errno, "open config file", ConfigFN);
    Config.Attach(cfgFD);
    // Now start reading records until eof.
    //
    XrdOucString nsin;
    XrdOucString nsout;

    while((var = Config.GetMyFirstWord())) {
      if (!strncmp(var, "xalien.", 7)) {
	var += 7;
	
	if (!strcmp("outputdir",var)) {
	  val = Config.GetWord();
	  XrdOucString creatdir="mkdir -p "; creatdir+=val; system(creatdir.c_str());
	  if ((!(val)|| (::access(val,R_OK|W_OK)))) {
	    return Eroute.Emsg("Config", errno, " access outputdir", val);
	  }
	  AlienOutputDir = val;
	}

	if (!strcmp("nworker",var)) {
	  val = Config.GetWord();
	  if ((!(val))) {
	    return Eroute.Emsg("Config", errno, "get number of workers", val);
	  }
	  AlienWorker = atoi(val);
	}

	if (!strcmp("perlmodule",var)) {
	  val = Config.GetWord();
	  if ((!(val)|| (::access(val,R_OK)))) {
	    return Eroute.Emsg("Config", errno, " access perl module", val);
	  }
	  AlienPerlModule = val;
	}	

	if (!strcmp("trace",var)) {
	  static struct traceopts {const char *opname; int opval;} tropts[] =
								   {
								     {"all",      TRACE_ALL},
								     {"close",    TRACE_close},
								     {"debug",    TRACE_debug},
								     {"open",     TRACE_open},
								     {"read",     TRACE_read},
								   };
	int i, neg, trval = 0, numopts = sizeof(tropts)/sizeof(struct traceopts);
	
	if (!(val = Config.GetWord())) {
	  Eroute.Emsg("Config", "trace option not specified"); return 1;
	}

	while (val) {
	  Eroute.Say("=====> xalien.trace: ", val,"");
	  if (!strcmp(val, "off")) trval = 0;
	  else {if ((neg = (val[0] == '-' && val[1]))) val++;
	    for (i = 0; i < numopts; i++)
	      {if (!strcmp(val, tropts[i].opname))
		  {if (neg) trval &= ~tropts[i].opval;
		    else  trval |=  tropts[i].opval;
		    break;
		  }
	      }
	    if (i >= numopts)
	      Eroute.Say("Config warning: ignoring invalid trace option '",val,"'.");
	  }
          val = Config.GetWord();
	}

	xAlienFsTrace.What = trval;
	}
      }
    }
  }

  XrdOucString nw=""; nw += AlienWorker;

  Eroute.Say("=====> xalien.outputdir: ",  AlienOutputDir.c_str(),"");
  Eroute.Say("=====> xalien.perlmodule: ", AlienPerlModule.c_str(),"");
  Eroute.Say("=====> xalien.worker: ", nw.c_str(),"");


  for (int i=0; i< AlienWorker; i++) {
    XrdOucString inpath  = AlienOutputDir;
    XrdOucString outpath = AlienOutputDir;
    XrdOucString inname  = "xai"; inname+=i;
    XrdOucString outname = "xoi"; outname+=i;

    AlienXO[i]  = XrdNetSocket::Create(eDest, inpath.c_str(),  inname.c_str(),  S_IRWXU,XRDNET_FIFO );
    if (!AlienXO[i]) {
      Eroute.Say("Config error: error connecting to socket ",inpath.c_str(),inname.c_str());
      NoGo=1;
    } else {
      Eroute.Say("=====> worker XI socket: ",  inpath.c_str(),inname.c_str());
    }
    
    AlienXI[i]  = XrdNetSocket::Create(eDest, outpath.c_str(), outname.c_str(), S_IRWXU, XRDNET_FIFO );
    if (!AlienXI[i]) {
      Eroute.Say("Config error: error connecting to socket ",outpath.c_str(), outname.c_str());
      NoGo=1;
    } else {
      Eroute.Say("=====> worker XO socket: ",  outpath.c_str(),outname.c_str());
      delete AlienXI[i];
      AlienXI[i] = 0;
    }
  }

  if (!NoGo) {
    return XrdOfs::Configure(Eroute);
  }

  return NoGo;
}
