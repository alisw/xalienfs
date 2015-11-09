#ifndef GAPI_JOB_OPERATIONS_H
#define GAPI_JOB_OPERATIONS_H

#include <string>
#include <map>
#include <vector>

typedef unsigned long GAPI_JOBID;

typedef struct {
  std::map<std::string, std::string> gapi_jobmap;
} GAPI_JOB;

typedef std::vector<GAPI_JOB> GAPI_JOBARRAY;

typedef std::string GAPI_JOBSTATUS_STRING;
typedef std::string GAPI_JOBFORMAT_STRING;

#ifdef __cplusplus
extern "C" {
#endif
  
  GAPI_JOBSTATUS_STRING gapi_jobstatusstring(std::string status,bool colour=true);
  GAPI_JOBFORMAT_STRING gapi_jobformatstring(std::string tag, std::string status, bool colour=true);
  GAPI_JOB *gapi_queryjob(GAPI_JOB);
  GAPI_JOBARRAY *gapi_queryjobs(std::string flags, std::string user, std::string sites, std::string nodes, std::string masterjobs, std::string order, std::string jobid, std::string limit, std::string sql);
  void gapi_printjobs(GAPI_JOBARRAY* gjobarray, std::string format, bool colour=true);

  GAPI_JOBID gapi_submit(const char* jdlurl, const char* args);
  int gapi_resubmit(GAPI_JOBID jobid);
  int gapi_kill(GAPI_JOBID jobid);
#ifdef __cplusplus
}
#endif

#endif  // #ifndef GAPI_JOB_OPERATIONS_H
