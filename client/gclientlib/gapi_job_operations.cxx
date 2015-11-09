#include "gapi_job_operations.h"
#include <string>
#include <sstream>
#include <iostream>
#include <errno.h>
#include <stdlib.h>
#include "gapiUI.h"
#include "../CODEC/CODEC.h"

#define MAX_JDL_SIZE 4*1024*1024
#define DEBUG_TAG "GAPI_JOB_OPERATIONS"

const char* PRINT_FORMAT_STANDARD[7] = {"user","queueId","priority","status","runtime","executable",NULL};
const char* PRINT_FORMAT_LONG[10]     = {"user","splitmode","queueId","priority","site","node","status","runtime","executable",NULL};
const char* PRINT_FORMAT_JDL[2]      = {"jdl",NULL};
const char* PRINT_FORMAT_TRACE[2]    = {"trace",NULL};
const char* PRINT_FORMAT_NONE[1]     = {NULL};

#include "debugmacros.h"

#include "posix_helpers.h"
using namespace PosixHelpers;

/** Gapi version for submission of local or catalogue jdl's
*/

std::string textnormal("\033[0m");
std::string textblack("\033[49;30m");
std::string textred("\033[49;31m");
std::string textrederror("\033[47;31m\e[5m");
std::string textblueerror("\033[47;34m\e[5m");
std::string textgreen("\033[49;32m");
std::string textyellow("\033[49;33m");
std::string textblue("\033[49;34m");
std::string textbold("\033[1m");
std::string textunbold("\033[0m");

GAPI_JOBID gapi_submit(const char* jdlurl, const char* args)
{
  char jdlbuffer[MAX_JDL_SIZE];

  if (!jdlurl) {
    std::cerr << "gapi_submit: you have to specify a jdl file" << std::endl;
    return 0;
  }

  bool nodelete=false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }

  GapiUI *gc = GapiUI::MakeGapiUI();

  std::string command("");

  if (urlIsLocal(std::string(jdlurl))) {
    std::string jdlstringurl = jdlurl;
    urlRemotePart(jdlstringurl);
    command = "localsubmit";

    // broken at the moment
    if (0)
    {
      std::cerr << "gapi_submit: cannot submit JDL from a local file - functionality broken at the moment!\n";
      if (!nodelete)delete gc;
      return 0;
    }

    FILE* jdlin = fopen(jdlstringurl.c_str(),"r");
    if (!jdlin) {
      std::cerr << "gapi_submit: cannot read your local jdl file " << jdlstringurl << std::endl;
      if (!nodelete)delete gc;
      return 0;
    }
    
    size_t jdlsize = fread(jdlbuffer, 1, MAX_JDL_SIZE,jdlin);
    if (jdlsize == MAX_JDL_SIZE) {
      std::cerr << "gapi_submit: your jdl file exceeds " << MAX_JDL_SIZE <<" bytes!" << std::endl;
      fclose(jdlin);
      if (!nodelete)delete gc;
      return 0;
   }
    
    fclose(jdlin);

    jdlbuffer[jdlsize] = 0;
   
    //    command +=  std::string(jdlbuffer);
    //    findandreplace(command, std::string("\"") , std::string("\\\""));
    setenv("GCLIENT_EXTRA_ARG",jdlbuffer,1);
    
  } else {
    command = "submit ";
    std::string jdl = jdlurl;
    urlRemotePart(jdl);
    command += jdl;
  }

  command += std::string(" ");
  command += std::string(args);

  std::cout<<"Submit " << command << std::endl;

  if(executeRemoteCommand(gc, command)){
    unsetenv("GCLIENT_EXTRA_ARG");
    if (!nodelete)delete gc;
    return 0;
  }

  unsetenv("GCLIENT_EXTRA_ARG");
  
  for(int column=0; ;column++){
    std::map<std::string, std::string> tags;
    gc->readTags(column, tags);
    if(!tags.size()) break;
    if (tags["jobId"].length()) {
      std::string sjobid = tags["jobId"];
      if (!nodelete)delete gc;
      return (GAPI_JOBID)(atoi(sjobid.c_str()));
    }
  };

  gc->PrintCommandStderr();
  if (!nodelete)delete gc;
  return 0;
}

/** Gapi version to kill grid jobs by jobid
    - return's 0 on success, 1 or -1 on error
*/

int gapi_kill(GAPI_JOBID jobid)
{
  bool nodelete=false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }

  GapiUI *gc = GapiUI::MakeGapiUI();
  char sjobid[512];
  sprintf(sjobid,"%d",(int)jobid);
  std::string command("");

  command = "kill ";
  command += std::string(sjobid);

  std::cout<<"gapi_kill: " << command << std::endl;

  if(executeRemoteCommand(gc, command)){
    if (!nodelete)delete gc;
    return -1;
  }

  gc->PrintCommandStderr();
  int returnresult = checkResult(gc);
  if (!nodelete)delete gc;
  return returnresult;

}

/** Gapi version to resubmit jobs by jobid
    - return's 0 on success, 1 or -1 on error
*/

int gapi_resubmit(GAPI_JOBID jobid)
{
  bool nodelete=false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }

  GapiUI *gc = GapiUI::MakeGapiUI();
  char sjobid[512];
  sprintf(sjobid,"%d",(int)jobid);
  std::string command("");

  command = "resubmit ";
  command += std::string(sjobid);

  std::cout<<"gapi_resubmit: " << command << std::endl;

  if(executeRemoteCommand(gc, command)){
    if (!nodelete)delete gc;
    return -1;
  }

  gc->PrintCommandStderr();
  int returnresult = checkResult(gc);
  if (!nodelete)delete gc;
  return returnresult;
}



/** Gapi version for interaction with the GRID job queue system

*/
GAPI_JOB *gapi_queryjob(GAPI_JOBID jobid)
{
  char sjobid[128];
  sprintf(sjobid,"%u",(int)jobid);
  std::string command("ps2 ");
  std::string job(sjobid);

  GAPI_JOB *gjob=NULL;

  bool nodelete=false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }
  GapiUI *gc = GapiUI::MakeGapiUI();

  command.append("- - - - - - - - ");
  command.append(job);
  command.append(" -");

  DEBUGMSG(3, << "Sending command: >" << command << "<" << std::endl);
  
  if(executeRemoteCommand(gc, command)){
    if (!nodelete)delete gc;
    return 0;
  }

  gjob = new GAPI_JOB;
  gc->readTags(0, gjob->gapi_jobmap);
  if(!gjob->gapi_jobmap.size()) {
    delete gjob;
    gjob = NULL;
  }
  if (!nodelete)delete gc;
  return gjob;
}

GAPI_JOBARRAY *gapi_queryjobs(std::string flags, std::string users, std::string sites, std::string nodes, std::string masterjobs, std::string order, std::string jobid, std::string limit, std::string sql){

  GAPI_JOBARRAY *gjobarray=NULL;

  bool nodelete=false;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }

  GapiUI *gc = GapiUI::MakeGapiUI();

  // prepare the AliEn ps2 command for job information
  std::stringstream argstrm;
  argstrm << "ps2 ";
  argstrm  << flags << " " << users << " " << sites << " " << nodes << " " << masterjobs << " " << order << " " << jobid << " " << limit << " " << sql;

  std::string command = argstrm.str();

  DEBUGMSG(3, << "Sending command: >" << command << "<" << std::endl);
  
  if(executeRemoteCommand(gc, command)){
    DEBUGMSG(3, << "Execution of command failed ... return 0." << std::endl;)
    if (!nodelete)delete gc;
    return 0;
  }

  gjobarray = new GAPI_JOBARRAY;
  int ncolumns = gc->GetStreamColumns(TBytestreamCODEC::kOUTPUT);
  if (ncolumns<=0) {
    delete gjobarray;
    gjobarray = NULL;
    if (!nodelete)delete gc;
    return NULL;
  }

  gjobarray->reserve(ncolumns);
  for(int column=0; column<ncolumns;column++){
    GAPI_JOB gjob;
    gc->readTags(column,  gjob.gapi_jobmap );
    gjobarray->push_back(gjob);


    DEBUGMSG(3, << "Reading Tags Column: >" << column << "<" << std::endl);
  };
  if (!nodelete)delete gc;
  return gjobarray;
}

void gapi_printjobs(GAPI_JOBARRAY* gjobarray, std::string format , bool colour){
  if (!gjobarray) 
    return;

  DEBUGMSG(3, << "gapi_printjobs: format >" << format << "< size >" << gjobarray->size() <<"<" << std::endl);

  for (int i=0; i< (int)gjobarray->size();i++) {
    DEBUGMSG(3, << "Print line: >" << i << "<" << std::endl);
    GAPI_JOB* gjob = &((*gjobarray)[i]);
    const char **pformat = PRINT_FORMAT_NONE;
    if (format=="") {
      pformat=PRINT_FORMAT_STANDARD;
    }
    if (format=="l") {
      pformat=PRINT_FORMAT_LONG;
    } 
    if (format=="x") {
    }
    if (format=="u") {
    }
    if (format=="t") {
    }
    if (format=="J") {
      if (colour)
	std::cout << textred;
      pformat=PRINT_FORMAT_JDL;
    }
    if (format=="T") {
      if (colour)
	std::cout << textblue;
      pformat=PRINT_FORMAT_TRACE;
    }
    
    for (int k=0;;k++) {
      if (pformat[k]==0)
	break;
      std::string pstring = gapi_jobformatstring(pformat[k], gjob->gapi_jobmap[pformat[k]],colour);
      
      
      // do the colour scheme for the priority display
      if (!strcmp(pformat[k],"priority")) {
	std::string field = gjob->gapi_jobmap["status"];
	if ( (!strcmp(field.c_str(),"INSERTING"))  || (!strcmp(field.c_str(),"WAITING")) ) {
	  int ipriority = atoi(gjob->gapi_jobmap["priority"].c_str());
	  if (ipriority <= 0) {
	    if (colour)
	      std::cout << textblueerror;
	  } else {
	    if (ipriority <70 ) {
	      if (colour)
		std::cout << textblue;
	    } else {
	      if (colour)
		std::cout << textgreen;
	    }
	  }
	} else {
	  if (colour)
	    std::cout << textnormal;
  	  pstring = " __ ";
	}
      }
      
      // do the colour scheme queueId highlighting
      if (!strcmp(pformat[k],"queueId")) {
	
	std::string field = gjob->gapi_jobmap["split"];
	if (atoi(field.c_str())) {
	  
	  std::cout << "-";
	  std::cout << pstring;
	} else {
	  if (colour)
	    std::cout << textbold;
	  std::cout << pstring;
	  std::cout << " ";
	  if (colour)
	    std::cout << textunbold;
	}
      } else {
	std::cout << pstring;
	if (colour)
	  std::cout << textnormal;
	//	std::cout << textunbold;
      }
    }
    std::cout << std::endl;
  }
  if (colour)
    std::cout << textnormal;
}

GAPI_JOBSTATUS_STRING gapi_jobstatusstring(std::string status , bool colour){
  DEBUGMSG(3, << "gapi_jobstatusstring: status >" << status << "<" <<" colour > " << colour << " <" << std::endl);
  std::string retstring = status;
  
  if (status == "KILLED") 
    if (colour)
      retstring = textred + "  K" + textnormal;
    else
      retstring = "  K";
  if (status == "RUNNING") 
    if (colour)
      retstring = textgreen + "  R" + textnormal;
    else 
      retstring = "  R";
  if (status == "STARTED") 
    if (colour)
      retstring = textgreen + " ST" + textnormal;
    else
      retstring = " ST";
  if (status == "DONE") 
    if (colour)
      retstring = textnormal + "  D" + textnormal;
    else
      retstring = "  D";
  if (status == "WAITING")
    if (colour)
      retstring = textblue + "  W" + textnormal;
    else
      retstring = "  W";
  if (status == "INSERTING")
    if (colour)
      retstring = textyellow + "  I" + textnormal;
    else
      retstring = "  I";
  if (status == "SPLIT") 
    retstring = "  S";
  if (status == "SPLITTING")
    retstring = " SP";
  if (status == "SAVING") 
    if (colour) 
      retstring = textgreen + " SV" + textnormal;
    else 
      retstring = " SV";
  if (status == "SAVED")
    retstring = "SVD";
  if (status == "ERROR_A")
    if (colour)
      retstring = textrederror + " EQ" + textnormal;
    else
      retstring = " EQ";
  if (status == "ERROR_E")
    if (colour)
      retstring = textrederror + " EE" + textnormal;
    else 
      retstring = " EE";
  if (status == "ERROR_I")
    if (colour)
      retstring = textrederror + " EI" + textnormal;
    else
      retstring = " EI";
  if (status == "ERROR_IB")
    if (colour)
      retstring = textrederror + "EIB" + textnormal;
    else
      retstring = "EIB";
  if (status == "ERROR_R")
    if (colour)
      retstring = textrederror + " ER" + textnormal;
    else
      retstring = " ER";
  if (status == "ERROR_S")
    if (colour)
      retstring = textrederror + " ES" + textnormal;
    else
      retstring = " ES";
  if (status == "ERROR_SV")
    if (colour)
      retstring = textrederror + "ESV" + textnormal;
    else
      retstring = "ESV";
  if (status == "ERROR_V")
    if (colour)
      retstring = textrederror + " EV" + textnormal;
    else
      retstring = " EV";
  if (status == "ERROR_VN")
    if (colour)
      retstring = textrederror + "EVN" + textnormal;
    else
      retstring = "EVN";
  if (status == "ERROR_VT")
    if (colour)
      retstring = textrederror + "EVT" + textnormal;
    else
      retstring = "EVT";
  if (status == "ERROR_SPLT")
    if (colour)
      retstring = textrederror + "ESP" + textnormal;
    else
      retstring = "ESP";
  if (status == "FAILED")
    if (colour)
      retstring = textrederror + " FF" + textnormal;
    else
      retstring = " FF";
  if (status == "ZOMBIE")
    if (colour)
      retstring = textblueerror + "  Z" + textnormal;
    else
      retstring = "  Z";

return retstring;
}

GAPI_JOBSTATUS_STRING gapi_jobformatstring(std::string tag, std::string field, bool colour){
  char hs[1024*1024]; 
  time_t tt = atoi(field.c_str());
  DEBUGMSG(3, << "gapi_jobformatstring: tag >" << tag << "<" << " field >" << field << "<" << std::endl);
  if (tag =="priority") {
    sprintf(hs,"%3s ",field.c_str());
  }
  if (tag =="execHost")
    sprintf(hs,"%-36s ",field.c_str());
  if (tag =="maxrsize")
    sprintf(hs,"%8s ",field.c_str());
  if (tag =="queueId") {
    sprintf(hs,"%7s ",field.c_str());
  }
  if (tag =="ncpu")
    sprintf(hs,"%2s ",field.c_str());
  if (tag =="cputime")
     sprintf(hs,"%8s ",field.c_str());
  if (tag =="executable") {
    int pos = field.rfind('/');
    int length = field.length();
    if (pos== (int)std::string::npos) {
      pos = 0;
    } else {
      pos++;
    }
    length -= pos;
    sprintf(hs,"%24s ",field.substr(pos,length).c_str());
  }
  if (tag =="split")
    sprintf(hs,"%7s ",field.c_str());
  if (tag =="cost")
    sprintf(hs,"%8s ",field.c_str());
  if (tag =="cpufamily")
    sprintf(hs,"%2s ",field.c_str());
  if (tag =="sent")  
    sprintf(hs,"%s ",ctime(&tt));
  if (tag =="finished")
    sprintf(hs,"%s ",ctime(&tt));
  if (tag =="started")
    sprintf(hs,"%s ",ctime(&tt));
  if (tag =="cpu")
    sprintf(hs,"%6s ",field.c_str());
  if (tag =="rsize")
    sprintf(hs,"%8s ",field.c_str());
  if (tag =="name")
    sprintf(hs,"%18s ",field.c_str());
  if (tag =="user")
    sprintf(hs,"%10s ",field.c_str());
  if (tag =="spyurl")
    sprintf(hs,"%36s ",field.c_str());
  if (tag =="commandArg")
    sprintf(hs,"%s ",field.c_str());
  if (tag =="runtime")
    sprintf(hs,"%8s ",field.c_str());
  if (tag =="mem")
    sprintf(hs,"%8s ",field.c_str());
  if (tag =="status")
    sprintf(hs,"%3s ",(gapi_jobstatusstring(field,colour)).c_str());
  if (tag =="splitting")
    sprintf(hs,"%12s ",field.c_str());
  if (tag =="cpuspeed")
    sprintf(hs,"%6s ",field.c_str());
  if (tag =="node")
    sprintf(hs,"%32s ",field.c_str());
  if (tag =="current")
    sprintf(hs,"%6s ",field.c_str());
  if (tag =="error")
    sprintf(hs,"%6s ",field.c_str());
  if (tag =="received")
    sprintf(hs,"%s ",ctime(&tt));      
  if (tag =="command")
    sprintf(hs,"%20s ",field.c_str());
  if (tag =="validate")
    sprintf(hs,"%2s ",field.c_str());
  if (tag =="splitmode") {
    if (field.length()) {
      sprintf(hs,"..%6s-split .. ",field.c_str());
    } else {
      sprintf(hs,"                  ");
    }
  }
  if (tag =="submitHost")
    sprintf(hs,"%48s ",field.c_str());
  if (tag =="runtimes")
    sprintf(hs,"%6s ",field.c_str());
  if (tag =="vsize")
    sprintf(hs,"%8s ",field.c_str());
  if (tag =="path")
    sprintf(hs,"%s ",field.c_str());
  if (tag =="jdl")
    sprintf(hs,"%s ",field.c_str());
  if (tag =="trace")
    sprintf(hs,"%s ",field.c_str());
  if (tag =="procinfotime")
    sprintf(hs,"%s ",ctime(&tt));
  if (tag =="maxvsize")
    sprintf(hs,"%8s ",field.c_str());
  if (tag =="site")
    sprintf(hs,"%24s ",field.c_str());
  if (tag =="expires")
    sprintf(hs,"%s ",ctime(&tt));
  return std::string(hs);
}
  
