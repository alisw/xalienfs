#include "ShmAuth.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iostream>

#include<config.h>

#define DEBUG_TAG "SHMAUTH"
#include "debugmacros.h"

TSharedAuthentication::TSharedAuthentication(bool master, const char* sharedid) {
    
  fUUbuffer = 0;
  fUUbuffersize = 0 ;
  // create the shared area
  fMaster = master;
  fdefaultlifetime=DEFAULTLIFETIME;

  char fMapfilename[4096];
  sprintf(fMapfilename,"%s/shmauth_%s.shmauth",getenv("HOME"),sharedid);

  fShmid = open(fMapfilename,O_CREAT | O_RDWR,S_IRWXU);
	    
  if(fShmid<0) {
      Fatal("Cannot open the shared memory mapped file!\n");
  }
  
  unsigned int offset=0;
  if (fMaster) {
    offset = (unsigned int) lseek(fShmid,0,SEEK_END);
    if (offset != MAXAUTHENTICATION*sizeof(struct StAuthblock)) {
      Info("Resizing the shared memory mapped file");
      // truncate the segment 
      ftruncate(fShmid,MAXAUTHENTICATION*sizeof(struct StAuthblock));
    }
  }

  offset = (unsigned int) lseek(fShmid,0,SEEK_END);
  if (offset != MAXAUTHENTICATION*sizeof(struct StAuthblock)) {
    Fatal("Cannot resize the shared map file to the needed size!\n");
  }
    
  // attach the shared memory segment
  fAuthblock = (struct StAuthblock*) mmap(0, offset ,PROT_WRITE|PROT_READ, MAP_SHARED,fShmid,0);
  

  if (!fAuthblock) {
    Fatal("Cannot map the shared memory file!\n");   
  }
}

////////////////////////////////////////////////////////////////////////////
// return the authentication block to a given sid
// Warning: block 0 is not used, it contains the total command count in cmdcnt
// and the number of total established connections in sid
////////////////////////////////////////////////////////////////////////////
struct TSharedAuthentication::StAuthblock* TSharedAuthentication::ReadBlock(unsigned int sid) {
    if (sid > MAXAUTHENTICATION) {
	Error("<sid> is out of allowed range");
	return NULL;
    } else {
	return &(fAuthblock[sid]);
    }
}
 
 
TSharedAuthentication::~TSharedAuthentication() {
    if (fAuthblock>0) {
	munmap(fAuthblock,MAXAUTHENTICATION*sizeof(struct StAuthblock));
    }
}
 
////////////////////////////////////////////////////////////////////////////
// dump all sids
////////////////////////////////////////////////////////////////////////////
void TSharedAuthentication::Dumpsids(){
    // loop over the authentication block
    unsigned int now = (unsigned int) time(NULL);
    for (unsigned int i = 1; i < MAXAUTHENTICATION; i++) {
      // this looks consistent with a valid sid block 
      if (fAuthblock[i].sid ==i ) {
	if (now > fAuthblock[i].exptime){
	  printf("SID: %04d TIME: %04d EXPIRED  USER: %16s IDLE: %04d IP: %d.%d.%d.%d\n",fAuthblock[i].sid,(now-fAuthblock[i].exptime),GetUser(i), GetIdletime(i), (GetIp(i)>>24) &0xff, (GetIp(i)>>16)&0xff, (GetIp(i)>>8)&0xff, (GetIp(i)&0xff));
	} else {
	  printf("SID: %04d TIME: %04d VALID    USER: %16s IDLE: %04d IP: %d.%d.%d.%d\n",fAuthblock[i].sid,(now-fAuthblock[i].exptime),GetUser(i), GetIdletime(i), (GetIp(i)>>24) &0xff, (GetIp(i)>>16)&0xff, (GetIp(i)>>8)&0xff, (GetIp(i)&0xff));
	}
      }
    }
}
 
///////////////////////////////////////////////////////////////////////////
// return ip of a session id
///////////////////////////////////////////////////////////////////////////

unsigned int TSharedAuthentication::GetIp(unsigned int i){
  if ( (i>=1) && (i<MAXAUTHENTICATION) ) {
    return (fAuthblock[i].ip);
  } else {
    return 0;
  }
}

///////////////////////////////////////////////////////////////////////////
// set ip for a session id
///////////////////////////////////////////////////////////////////////////

void TSharedAuthentication::SetIp(unsigned int i, unsigned int ip){
  if ( (i>=1) && (i<MAXAUTHENTICATION) ) {
    fAuthblock[i].ip = ip;
  }
}

///////////////////////////////////////////////////////////////////////////
// return rest of the token lifetime
///////////////////////////////////////////////////////////////////////////

int TSharedAuthentication::GetLifetime(int i){
  unsigned int now = (unsigned int ) time(NULL);
  if ( (i>=1) && (i<MAXAUTHENTICATION) ) {
    return (fAuthblock[i].exptime-now);
  } else {
    return 0;
  }
}

///////////////////////////////////////////////////////////////////////////
// return idle time
///////////////////////////////////////////////////////////////////////////

int TSharedAuthentication::GetIdletime(int i){
  unsigned int now = (unsigned int ) time(NULL);
  if ( (i>=1) && (i<MAXAUTHENTICATION) ) {
    return (now-fAuthblock[i].actime);
  } else {
    return 0;
  }
}


////////////////////////////////////////////////////////////////////////////
// return the number of interactive sessions
////////////////////////////////////////////////////////////////////////////
unsigned int TSharedAuthentication::GetActiveSessions() {
  unsigned int nsessions;
  nsessions=0;
  for (unsigned int i = 1; i < MAXAUTHENTICATION; i++) {
    if (fAuthblock[i].sid == i ) {
      if (GetIdletime(i) < 300) {
	nsessions++;
      }
    }
  }
  return nsessions;
}

////////////////////////////////////////////////////////////////////////////
// dump the user information
////////////////////////////////////////////////////////////////////////////
void TSharedAuthentication::WriteUserInfoFile(const char* filename) {
  FILE* fd;
  char tmpfilename[4096];
  // loop over the authentication block
  unsigned int now;
  now = (unsigned int) time(NULL);
  if (!filename) 
    return;
  sprintf(tmpfilename,"%s.tmp",filename);

  fd = fopen(tmpfilename,"w+");

  // write the summary field
  fprintf(fd,"total connections %d cmd %d \n", fAuthblock[0].sid, fAuthblock[0].cmdcnt );
  for (unsigned int i = 0; i < MAXAUTHENTICATION; i++) {
    // this looks consistent with a valid sid block 
    if (fAuthblock[i].sid ==i ) {
      if (now > fAuthblock[i].exptime){
	fprintf(fd,"user %16s idle %04d cmd %06d expired ip %d.%d.%d.%d\n",GetUser(i), GetIdletime(i), GetCmdcnt(i), (GetIp(i)>>24) &0xff, (GetIp(i)>>16)&0xff, (GetIp(i)>>8)&0xff, (GetIp(i)&0xff));
      } else {
	fprintf(fd,"user %16s idle %04d cmd %06d valid   ip %d.%d.%d.%d\n",GetUser(i), GetIdletime(i), GetCmdcnt(i), (GetIp(i)>>24) &0xff, (GetIp(i)>>16)&0xff, (GetIp(i)>>8)&0xff, (GetIp(i)&0xff));
      }
    }
  }
  fclose(fd);
  rename(tmpfilename,filename);
}

////////////////////////////////////////////////////////////////////////////
// return the number of valid sessions
////////////////////////////////////////////////////////////////////////////
unsigned int TSharedAuthentication::GetNSessions() {
  unsigned int now;
  unsigned int nsessions;
  nsessions=0;
  now = (unsigned int) time(NULL);
  for (unsigned int i = 1; i < MAXAUTHENTICATION; i++) {
    if (fAuthblock[i].sid == i ) {
      if (now < fAuthblock[i].exptime){
	nsessions++;
      }
    }
  }
  return nsessions;
}

////////////////////////////////////////////////////////////////////////////
// return a free sid
////////////////////////////////////////////////////////////////////////////
unsigned int TSharedAuthentication::Getsid(){
    // loop over the authentication block, slay old sid's and return a free one
    unsigned int now;
    now = (unsigned int) time(NULL);
    for (unsigned int i = 1; i < MAXAUTHENTICATION; i++) {
	if (fAuthblock[i].sid == i ) {
	    // this looks consistent with a valid sid block 
	    if (now > fAuthblock[i].exptime){
		// this session timed out -> free it
		fAuthblock[i].sid = i;
		// reserve this block for the next DEFAULTLIFETIME seconds to create
		// a valid session, otherwise it can be reused
		fAuthblock[i].exptime = now + fdefaultlifetime;
		fAuthblock[i].cmdcnt  = 0;
		fAuthblock[i].ip = 0;
		//CreateNonce(fAuthblock[i].nonce);
		IncrementSessionTotal();
		return i;
	    }
	} else {
	    // this block is still unused, reserve and give it
	    fAuthblock[i].sid = i;
	    fAuthblock[i].exptime = now + fdefaultlifetime;
	    fAuthblock[i].cmdcnt  = 0;
	    fAuthblock[i].ip = 0;
	    //CreateNonce(fAuthblock[i].nonce);
	    IncrementSessionTotal();
	    return i;
	}
    }
    // if there is no expired or free block, let's offer the one with the longest idle time
    unsigned int maxidle;
    unsigned int maxidle_i;
    unsigned int idle;
    maxidle=0;
    maxidle_i=0;
    idle=0;
    for (unsigned int i = 1; i < MAXAUTHENTICATION; i++) {
      idle = GetIdletime(i);
      if (idle > maxidle) {
	maxidle=idle;
	maxidle_i = i;
      }
    }
    fAuthblock[maxidle_i].sid = maxidle_i;
    fAuthblock[maxidle_i].exptime = now + fdefaultlifetime;
    fAuthblock[maxidle_i].cmdcnt  = 0;
    fAuthblock[maxidle_i].ip = 0;
    IncrementSessionTotal();
    return maxidle_i;
}
 
////////////////////////////////////////////////////////////////////////////
// Authorize a token for a sid
////////////////////////////////////////////////////////////////////////////
bool TSharedAuthentication::Authorize(unsigned int sid, unsigned int token) {
  //    unsigned int now = time(NULL);
    if (token == fAuthblock[sid].token) 
      return true;
    else 
      return false;
}

/**
   fills an allocated 16 byte nonce where the first 8 bytes are
   random and the following 8 bytes represent a counter

   @param nonce (out) preallocated pointer to the nonce

   @return 0 in case of success (currently always succeeds)
*/
int TSharedAuthentication::CreateNonce(unsigned char *nonce) {

  //TODO CreatePassword, length argument needs +1 syntax?
  CreatePassword((char*)nonce,9);
  memset(&nonce[9],0,7);
  CreatePassword((char*)&nonce[15],2);
  DEBUGHEXDUMP(8,<< "Created Nonce:", nonce, SHMAUTH_NONCE_SIZE);
  return 0;
}

/**
   increments the part of the given nonce which is used as counter by one.

   @param nonce pointer to the nonce

   @return 0 in case of success (currently always succeeds)
*/
int TSharedAuthentication::IncrementNonce(unsigned char *nonce) {
  //return(0);
  if(!++nonce[15]) if(!++nonce[14]) if(!++nonce[13]) if(!++nonce[12])
   if(!++nonce[11]) if(!++nonce[10]) if(!++nonce[9]) if(!++nonce[8]) {
     // if we get here we are out of nonces - BAD!
   }
  DEBUGHEXDUMP(8,<< "Incremented Nonce:", nonce, SHMAUTH_NONCE_SIZE);
  return(0);
}

/**
   returns an allocated string where the nonce bytes have been converted
   to space separated numbers. Used for debugging.
   The returned array of char has to be freed via delete[]!

   @param pointer to the nonce

   @return the allocated string containing the nonce
*/
char *TSharedAuthentication::NonceToString(unsigned char *nonce) {
  char buffer[6];
  char *noncestr = new char[SHMAUTH_NONCE_SIZE*5];
  if(nonce==NULL) {
    fprintf(stderr,"Error: (%s) nonce points to NULL\n",__FUNCTION__);
    return(NULL);
  }
  strcpy(noncestr,"");
  for(int i=0;i<SHMAUTH_NONCE_SIZE;i++) {
    sprintf(buffer,"%d ",(int) nonce[i]);
    strcat(noncestr,buffer);
  }
  return noncestr;
}

/**
   converts string representation of the nonce to byte representation.
   Used when reading back a token file.

   @param str string containing the nonce bytes as space separated numbers.
   @param nonce (out) pointer to the nonce

   @return 0 for success, -1 in case of error
*/
int TSharedAuthentication::StringToNonce(char* str, unsigned char *nonce) {
  char *strptr=str;
  char chr[5];
  int check=0;
  if(str==NULL) {
    fprintf(stderr,"Error: %s str is NULL\n",__FUNCTION__);
    return(-1);
  }

  for(int i=0;i<SHMAUTH_NONCE_SIZE;i++) {
    check+=sscanf(strptr," %3s ",chr);
    while(*strptr==' ') strptr++;
    nonce[i]=(unsigned char) atoi(chr);
    strptr+=strlen(chr);
  }
  if(check!=SHMAUTH_NONCE_SIZE)
    return(-1);

  return(0);
}

/**
   prints nonce to stdout

   @param nonce pointer to nonce
 */
int TSharedAuthentication::PrintNonce(unsigned char *nonce) {
  char *str=NonceToString(nonce);
  printf("nonce: (%p) %s\n",nonce,str);
  delete[] str;
  return(0);
}
