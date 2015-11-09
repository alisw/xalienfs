#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "C2Perl/C2PERL.h"
#include "XrdNet/XrdNetOpts.hh"
#include "XrdNet/XrdNetSocket.hh"
#include "XrdOuc/XrdOucString.hh"
#include "XrdSys/XrdSysLogger.hh"
#include "XrdSys/XrdSysError.hh"

int serverpids[100];
int nserver=0;
int flag_terminate=0;
int sig_fd=0;

void caught_sigpipe(int signal) {
  std::cout << "DEBUG: " << getpid() << " caught SIGPIPE\n";
}

void caught_sigalarm(int signal) {
  int message = -ETIMEDOUT;
  // send a 0 to indicate that there is no result!
  std::cout << "DEBUG: " << getpid() << " caught SIGALARM\n";
  if ( (::write(sig_fd, &message, sizeof(message))) != sizeof(message) ) {
    fprintf(stderr,"error: cannot respond on outsocket after sig_alarm\n");
  }
  fprintf(stderr,"error: process %d caught SIGALARM\n",getpid());
  exit(-2);
}

void settermination(int signal) {
  switch(signal) {
  case SIGPIPE:
    std::cout << "DEBUG: " << getpid() << " caught SIGPIPE\n";
    break;;
  case SIGINT:
    std::cout << "DEBUG: " << getpid() << " caught SIGINT\n";
    break;
  case SIGTERM:
    std::cout << "DEBUG: " << getpid() << " caught SIGTERM\n";
    break;
  }

  flag_terminate=1;
}

int set_sigalarm() {
  struct sigaction act,oldact;
  sigset_t sset,oldset;

  if(sigemptyset(&sset)<0) {
    fprintf(stderr,"unable to execute sigemptyset\n");
    return 0;
  }

  if(sigaddset(&sset,SIGALRM)<0) {
    fprintf(stderr,"error adding SIGALRM to sigset\n");
    return 0;
  }

  if(sigprocmask(SIG_UNBLOCK,&sset,&oldset)<0) {
    fprintf(stderr,"error setting sigprocmask\n");
    return 0;
  }

  act.sa_handler=&caught_sigalarm;

  if(sigaction(SIGALRM,&act,&oldact)<0) {
    fprintf(stderr,"error setting signal handler for SIGALRM");
    return 0;
  }

  return 1;
}


int makeWorker(const char* module, const char* pipedir, int n) {
  int pid;
  printf("Creating Worker %s::%s::%d\n",module,pipedir,n);
  if (!(pid=fork())) {
    C2PERLInit(module);
    XrdOucString loggerpath = pipedir; loggerpath+="/xa"; loggerpath+=n; loggerpath+=".log";
    XrdOucString inpath = pipedir; 
    XrdOucString inname = "xai"; inname+=n;
    XrdOucString outpath = pipedir; 
    XrdOucString outname = "xao"; outname+=n;
    stdout = stderr;
    XrdSysLogger* logger = new XrdSysLogger();
    logger->Bind(loggerpath.c_str(),0);
    XrdSysError eDest(logger);
    XrdNetSocket* socket    = XrdNetSocket::Create(&eDest, inpath.c_str(),  inname.c_str(),  S_IRWXU, XRDNET_FIFO);
    XrdNetSocket* outsocket = XrdNetSocket::Create(&eDest, outpath.c_str(), outname.c_str(), S_IRWXU, XRDNET_FIFO);
    char* inputbuffer = (char*) malloc(1024*1024);

    for (int l=0; l < 2000; l++) {
      int messagelen=0;
      if ( (read(socket->SockNum(), &messagelen,sizeof(messagelen))) != sizeof(messagelen)) {
	fprintf(stderr,"error: read from communication pipe failed - couldn't receive message len - teminating!\n");
	exit(-1);
      }
      fprintf(stdout,"Info: received msg len %d\n",messagelen);
      char uuidstring[40];
      if ( (read(socket->SockNum(), uuidstring,sizeof(uuidstring))) != sizeof(uuidstring)) {
	fprintf(stderr,"error: read from communication pipe failed - couldn't receive uuidstring - teminating!\n");
	exit(-1);
      }
      uuidstring[40]=0;
      fprintf(stdout,"Info: received uuid %s\n",uuidstring);
      char zerouser[64];
      if ( (read(socket->SockNum(), zerouser,sizeof(zerouser))) != sizeof(zerouser)) {
	fprintf(stderr,"error: read from communication pipe failed - couldn't receive user - teminating!\n");
	exit(-1);
      }
      fprintf(stdout,"Info: received user %s\n",zerouser);
      ssize_t iblen = messagelen;
      if ( iblen < (ssize_t)sizeof(inputbuffer) ) {
	// we don't have enough space ... that is really strange ...
	fprintf(stderr,"error: read from communication pipe impossible - input string too long!\n");
	continue;
      }
			      
      if ( (read(socket->SockNum(), inputbuffer,iblen)) != iblen) {
	fprintf(stderr,"error: read from communication pipe failed - couldn't receive inputbuffer - terminating!\n");
	exit(-1);
      }
      fprintf(stdout,"Info: received inputstream %s\n",inputbuffer);

      char* outbyteresult = "";

      sig_fd = outsocket->SockNum();
      set_sigalarm();
      alarm(10);
      C2PERLResult_t *res;
      if (!strcmp(zerouser,"LocalCall")) {
	// this is a local call out for authentication .... the three arguments come as <arg1>:<arg2>:<arg3> !
	XrdOucString t1,t2,t3;
	t1 = inputbuffer;
	t2 = inputbuffer;
	t3 = inputbuffer;
	int p1,p2;
	p1 = t1.find(":"); t2.erase(0,p1+1);
	t3 = t2;
	p2 = t2.find(":"); t3.erase(0,p2+1);
	t1.erase(p1);
	t2.erase(p2);
	fprintf(stdout,"Calling %s %s %s\n",t1.c_str(),t2.c_str(),t3.c_str());
	res=C2PERLCall("LocalCall",3,t1.c_str(),t2.c_str(),t3.c_str());
      } else {
	// this is a standard RPC
	res=C2PERLCall("Call",2,zerouser,inputbuffer);
      }
      alarm(0);

      if ((outbyteresult= (char *) C2PERLFetchResult(res))==NULL) {
	fprintf(stderr,"error: fetch result failed\n");
	int message = 0;
	// send a 0 to indicate that there is no result!
	if ( (::write(outsocket->SockNum(), &message, sizeof(message))) != sizeof(message) ) {
	  fprintf(stderr,"error: cannot respond on outsocket\n");
	  exit(-1);
	}
	continue;
      }
      int outbytelen = strlen(outbyteresult)+1;

      fprintf(stdout, "Sending %lu\n",sizeof(outbytelen));
      // send the result through the output pipe
      if ( (::write(outsocket->SockNum(), &outbytelen, sizeof(outbytelen))) != sizeof(outbytelen) ) {
	  fprintf(stderr,"error: cannot respond on outsocket\n");
	  exit(-1);
      }

      if ( (::write(outsocket->SockNum(), uuidstring,sizeof(uuidstring))) != sizeof(uuidstring)) {
	fprintf(stderr,"error: write communication pipe failed - couldn't send uuidstring - teminating!\n");
	exit(-1);
      }

      
      fprintf(stdout,"Sending Outbyteresult of len %d\n",outbytelen);
      if ( (write(outsocket->SockNum(),outbyteresult, outbytelen) ) != outbytelen) {
	fprintf(stderr,"error: cannot write output to outsocket");
	exit(-1);
      }
      
      fprintf(stdout, "Send completed!\n");
      // done
      if (res) 
	C2PERLFreeResult(res);
      continue;
    } 

    fprintf(stdout,"Info: retiring myself");
    exit(0);
  } 
  return pid;
}

int main(int argc, char* argv[]) {
  if (argc <4) {
    fprintf(stderr,"error: usage %s <perl-module> <inputpipedir> <nserver>!\n", argv[0]);
    exit(-1);
  }
  const char* perlmodule = argv[1];
  const char* inputpipe =  argv[2];
  nserver = atoi(argv[3]);

  if (nserver >100) {
    nserver =100;
  }
  if (nserver <0) {
    nserver =1;
  }

  pid_t pid=0;
  int chldstatus;
  
  // install signal handler for graceful termination of all processes
  struct sigaction act,oldact;
  sigset_t sset,oldset;

  if(sigemptyset(&sset)<0)
    fprintf(stderr,"unable to execute sigemptyset\n");
  if(sigaddset(&sset,SIGTERM)<0)
    fprintf(stderr,"error adding SIGTERM to sigset\n");
  if(sigaddset(&sset,SIGINT)<0)
    fprintf(stderr,"error adding SIGINT to sigset\n");
  if(sigaddset(&sset,SIGPIPE)<0)
    fprintf(stderr,"error adding SIGPIPE to sigset\n");
  if(sigprocmask(SIG_UNBLOCK,&sset,&oldset)<0)
    fprintf(stderr,"error setting sigprocmask\n");
  act.sa_handler=&settermination;
  act.sa_flags=0;
  if(sigaction(SIGTERM,&act,&oldact)<0)
    fprintf(stderr,"error setting signal handler");
  if(sigaction(SIGINT,&act,&oldact)<0)
    fprintf(stderr,"error setting signal handler");
  

  // create initial children
  for (int i=0; i<nserver;i++) {
    serverpids[i]=makeWorker(perlmodule,inputpipe,i);
  }

  while(1) {
    // keep children up eternally
    if((pid=waitpid(0,&chldstatus,WNOHANG)) > 0) {
      for(int i=0;i<nserver;i++) {
	if(pid==serverpids[i]) {
	  XrdOucString workerpipe = inputpipe;
	  workerpipe+= "xaw";workerpipe+=i;
	  XrdOucString workeroutpipe = inputpipe;
	  workeroutpipe+= "xao";workeroutpipe+=i;
	  fprintf(stderr, "reaped child %d (pid=%d)\n", i, pid);
	  sigaction(SIGTERM,&oldact,NULL);
	  serverpids[i]=makeWorker(perlmodule,inputpipe,i);
	  sleep(1);
	  sigaction(SIGTERM,&act,NULL);
          fprintf(stderr, "Forked SlaveServer %d (pid=%d)\n" , i, serverpids[i]);
          break;
	}
      }
    }
    if (flag_terminate) {
      break;
    }
    usleep(10000);
  }

  for (int i=0; i< nserver; i++) {
    if (serverpids[i]>0) {
      if (!kill(serverpids[i],0)) {
	fflush(stdout);
	kill(serverpids[i],SIGKILL);
	waitpid(serverpids[i],NULL,0);
      }
    }
  }
  exit(0);
}
      
