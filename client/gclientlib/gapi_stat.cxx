#include "gapi_stat.h"
#include <string>
#include <errno.h>
#include <stdlib.h>
#include <pwd.h>
#include <grp.h>
#include <list>
#include <libgen.h>

#include "gclient.h"
#include "posix_helpers.h"
#include "gapi_dir_operations.h"

using namespace PosixHelpers;

#define DEBUG_TAG "GAPI_STAT"
#include "debugmacros.h"

std::list <struct GAPI_STAT_NODE*> gapi_stat_cache;

static int doStat(const std::string &cmd)
{
  bool nodelete;
  if (GapiUI::GetGapiUI() ) {
    nodelete = true;
  }
  GapiUI *gc = GapiUI::MakeGapiUI();
  
  if(!gc){
    errno=ENOMEM;
    return -1;
  }

  if(!gc->Connected()) {
    errno=ECONNREFUSED;
    if (!nodelete)delete gc;
    return -1;
  }

  std::string command(cmd.c_str());

  DEBUGMSG(3, << "Sending command: >" << command << "<" << std::endl);

  if(!gc->Command(command.c_str())) {
    t_gerror gerror = gc->GetErrorType();
    if(gc->ErrorMatches(gerror,"server:authentication")) {
      errno=EACCES;
      if (!nodelete) delete gc;
      return -1;
    }
    if(gc->ErrorMatches(gerror,"server:session_expired")) {
      errno=ECONNREFUSED;
      if (!nodelete) delete gc;
      return -1;
    }
    if (!nodelete) delete gc;
    errno = EAGAIN;
    return -1;
  }

  std::map<std::string, std::string> tags;

  // take the time of the last stat
  time_t now = time(NULL);


  int column=0;
  for(column=0; ;column++){
    gc->readTags(column, tags);
    if(!tags.size()) {
      break;
    }

    GAPI_STAT* st = new GAPI_STAT;
    memset(st, 0, sizeof(GAPI_STAT));
    // Read tag values
    std::string line, perms, user, group, date, name, path, fullpath;
    unsigned long long blocks, size, ctime;

    group=tags["group"];
    perms=tags["permissions"];
    user=tags["user"];
    name=tags["name"];
    size=atoll(tags["size"].c_str());
    blocks=size/1024;
    struct tm ts;
    std::string extdate = "2006 ";
    extdate += tags["date"];
    //    char* ptr=strptime(extdate.c_str(),"%Y %b %d %H:%M",&ts);
    //    printf("processed %d\n",ptr-tags["date"].c_str());
    //    ctime=atoll(tags["date"].c_str());
    ctime = mktime(&ts);
    //    printf("Converting |%s| to %u\n",extdate.c_str(),ctime);
    path=tags["path"];
    fullpath = path + name;

    // Fill stat structure
    st->st_size=size;
    struct passwd *pw=getpwnam(user.c_str());
    if(pw)
      st->st_uid = pw->pw_uid;
    else
      st->st_uid = 0;
    struct group *gp=getgrnam(user.c_str());
    if(gp)
      st->st_gid = gp->gr_gid;
    else
      st->st_gid = 0;
  
    st->st_mode=0;
    switch(perms[0]){
    case 'd': st->st_mode |= S_IFDIR; 
      break;
    case 'l': st->st_mode |= S_IFLNK;
      break;
    default:
      st->st_mode |= S_IFREG;
    }

    if(perms[1] == 'r') st->st_mode |= S_IRUSR;
    if(perms[4] == 'r') st->st_mode |= S_IRGRP;
    if(perms[7] == 'r') st->st_mode |= S_IROTH;
    if(perms[2] == 'w') st->st_mode |= S_IWUSR;
    if(perms[5] == 'w') st->st_mode |= S_IWGRP;
    if(perms[8] == 'w') st->st_mode |= S_IWOTH;
    if(perms[3] == 'x') st->st_mode |= S_IXUSR;
    if(perms[6] == 'x') st->st_mode |= S_IXGRP;
    if(perms[9] == 'x') st->st_mode |= S_IXOTH;
    
#if defined(_NETBSD_SOURCE)
    st->st_atimespec.tv_sec =st->st_mtimespec.tv_sec=st->st_ctimespec.tv_sec;
    st->st_atimespec.tv_nsec=st->st_mtimespec.tv_sec=st->st_ctimespec.tv_sec;
#else
    st->st_atime=st->st_mtime=st->st_ctime=ctime;
#endif

    // find the file in the cache list
    list<struct GAPI_STAT_NODE*>::iterator iter;
    list<struct GAPI_STAT_NODE*>::iterator itercopy;
    bool found=false;
    // Iterate through list and output each element.

    iter=gapi_stat_cache.begin();
    while (iter != gapi_stat_cache.end()) {
      //    for (iter=gapi_stat_cache.begin(); iter != gapi_stat_cache.end(); iter++) {
      if (!strcmp((*iter)->pathname,fullpath.c_str())) {
	// this is the same entry, update it
	(*iter)->expires = now + gapi_stat_cachetime;
	(*iter)->st = st;
	found=true;
	iter++;
      } else {
	if (now > (*iter)->expires) {
	  // printf("Removing Object %s\n",(*iter)->pathname);
	  // remove the GAPI_STAT object
	  // delete (*iter)->st;
	  // (*iter)->st=0;
	  // remote the entry from the list, it is expired
	  itercopy = iter;
	  iter++;
	  gapi_stat_cache.erase((itercopy));
	} else {
	  iter++;
	}
      }
    }

    if (!found) {
      struct GAPI_STAT_NODE* newnode = new struct GAPI_STAT_NODE;
      newnode->st = st;
      newnode->expires = now + gapi_stat_cachetime;
      strcpy(newnode->pathname, fullpath.c_str());
      // add to the stat cache
      gapi_stat_cache.push_back(newnode);
    }
  }

  if (!nodelete)delete gc;
  return 0;
}


/** Gapi version of the stat command. See <tt>man stat</tt> for the
    general idea.
    On success, zero is returned.  On error, -1 is returned, and
    errno is set appropriately. Additional error codes are:
    - EACCES: Authentication failed
    - ECONNREFUSED: You need to (re)connect to the gapi server
    - ENOENT: the file is not existing
*/
int gapi_stat(const char *file_name, GAPI_STAT *st)
{  
  std::string fileName=file_name;

  if(urlIsLocal(fileName)){
    DEBUGMSG(3, << "Doing local stat: >" << std::endl);    
    return stat(fileName.c_str(), (struct stat *)st); 
  }
  
  DEBUGMSG(3, << "Doing stat for: >" << file_name << std::endl);    
  urlRemotePart(fileName);
  time_t now = time(NULL);
  
  // shared library blocker
  //  if (fileName.find(std::string(".so.")) != std::string::npos) {
  //    printf("Shared library blocked\n");
  //    return -EPERM;//ENOENT
  //  }

  if (fileName.c_str()[0] != '/') {

    // this is a relative path, convert it to an absolut path
    char cwd[4096];
    std::string newfileName = std::string(gapi_getcwd(cwd, 4096)) + std::string("/") + fileName;
    fileName = newfileName;
  }


  // if it is the root path
  if (fileName == "/") {
    memset(st,0,sizeof(struct stat));
    st->st_dev=0;      /* device */
    st->st_ino=0;      /* inode */
    st->st_mode=0xfff|S_IFDIR; /* protection */
    st->st_nlink=24;    /* number of hard links */
    st->st_uid=0;      /* user ID of owner */
    st->st_gid=0;      /* group ID of owner */
    st->st_rdev=0;     /* device type (if inode device) */
    st->st_size=7;     /* total size, in bytes */
    st->st_blksize=1024;  /* blocksize for filesystem I/O */
    st->st_blocks=4096;   /* number of blocks allocated */
    time_t now = time(NULL);
    st->st_atime=now;    /* time of last access */
    st->st_mtime=now;    /* time of last modification */
    st->st_ctime=now;    /* time of last change */
    DEBUGMSG(3, << "Stat on ROOT: > sizeof(stat)=" << sizeof(struct stat) << std::endl);

    return 0;
  }

  // try to find in the cache list
  {
    list<struct GAPI_STAT_NODE*>::const_iterator iter;
    // Iterate through list and output each element.
    for (iter=gapi_stat_cache.begin(); iter != gapi_stat_cache.end(); iter++) {
      if (!strcmp((*iter)->pathname,fileName.c_str())) {  
	if ( now > (*iter)->expires) {
	  break;
	} else {
	  memcpy(st, (*iter)->st, sizeof (GAPI_STAT));
	  return 0;
	}
      }
    }
  }

  char dir2stat[4096];
  strcpy(dir2stat,fileName.c_str());
  std::string command = "ls -l ";
  command.append(dirname(dir2stat));
  int ret = doStat(command.c_str()); 

  if(ret) return ret;

  // try to find it now in the cache list
  {
    list<struct GAPI_STAT_NODE*>::const_iterator iter;
    // Iterate through list and output each element.
    for (iter=gapi_stat_cache.begin(); iter != gapi_stat_cache.end(); iter++) {
      if (!strcmp((*iter)->pathname,fileName.c_str())) {  
	if ( now > (*iter)->expires) {
	  break;
	} else {
	  memcpy(st, (*iter)->st, sizeof (GAPI_STAT));
	  return 0;
	}
      }
    }
  }
  // there is no such entry ....
  errno=ENOENT;
  return -1;
}

int gapi_lstat(const char *file_name, GAPI_STAT *st)
{
  return gapi_stat(file_name,st);
}

int gapi_access(const char *filepath, int mode) {
  GAPI_STAT st;

  if ((!mode) || (mode & 0x4)) {
    if (!gapi_stat(filepath,&st)){
      return 0;
    } else {
      return -1;
    }
  } 

  /* default say access is OK */
  return 0;
}

void  gapi_perror(const char *msg) {};

char *gapi_serror() {return (char*)"gapi_serror: not implemented";}
