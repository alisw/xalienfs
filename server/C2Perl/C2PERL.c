#include <EXTERN.h>
#include <perl.h>

#define C2PERL_INTERNAL
#include "C2PERL.h"
#include <stdarg.h>
#include <string.h>

int g_init;

static PerlInterpreter *my_perl;

static void xs_init (void);
int g_connect=0;
EXTERN_C void boot_DynaLoader (CV* cv);

/**
   This function is passed into the perl_parse function and enables
   the Perl interpreter object to dynamically load shared libraries
   which may be connected with some 'required' Perl modules.
 */
EXTERN_C void xs_init()
 { 
   char *file = __FILE__; 
   /* DynaLoader is a special case */ 
   newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file); 
 }

/**
   Loads a Perl module and compiles it

   @param perlmodule filename of the Perl module

   @return 1 in case of success (currently always succeeds)
 */
int C2PERLInit(const char *perlmodule) 
///////////////////////////////////////////////////////////////////////////
{
  char *cmds[] = { "",NULL};

  char perl_api[255];
  sprintf(perl_api,"%s",perlmodule);

  cmds[1] = perl_api;

  //TODO: better way to get library location
  if(getenv("GSHELL_ROOT")==NULL) {
    fprintf(stderr,"Error: GSHELL_ROOT not set\n");
    g_init = 0;
    return(g_init);
  } else {
    char perllib[400];
    char *gloc = getenv("GSHELL_ROOT");

    strcpy(perllib,"PERL5LIB=");
    strncat(perllib,gloc,160);
    strcat(perllib,"/../lib/perl5/site_perl:");
    strncat(perllib,gloc,160);
    strcat(perllib,"/../lib/perl5");
 
    
    if(putenv(perllib)<0) {
      fprintf(stderr,"Error: Could not set GSHELL_ROOT\n");
      g_init = 0;
      return(g_init);
    }
    //printf("PERL5LIB set to %s\n",perllib);
  }

  my_perl = perl_alloc();

  PL_perl_destruct_level = 1;  
  perl_construct(my_perl);

   

  perl_parse(my_perl, xs_init, 2, cmds, NULL);
/*  PL_exit_flags |= PERL_EXIT_DESTRUCT_END;*/

  perl_run(my_perl);

  g_init = 1;
  return  (g_init); 
}

/**
   returns whether the Perl parser has already been initialized
   with a Perl module.

   @return 1 for initialized, 0 for not initialized
*/
int C2PERLIsInitialized()
{
  if (g_init>0)
    return 1;
  else
    return 0;
}

/**
   Destroys the Perl interpreter object and frees memory

   @return 0 for success, -1 in case of error
*/
///////////////////////////////////////////////////////////////////////////
int C2PERLClose() 
///////////////////////////////////////////////////////////////////////////
{
  if (!g_init)
    {
      fprintf (stderr, "Error: C2PERL not initialized\nPlease, do C2PERLInit\n");
      return -1;
    }

  perl_destruct(my_perl);
  perl_free(my_perl); 

  g_init=0;

  return(0);
}

/**
   TODO: this seems to offer the exact functionality of the above
   C2PERLClose() function. One of these should be dropped.
*/
///////////////////////////////////////////////////////////////////////////
int C2PERLPerlDestroy()
///////////////////////////////////////////////////////////////////////////
{
    perl_destruct(my_perl);
    perl_free(my_perl);
    g_init=0;
    return(0);
}

/**
   Executes a Perl function and returns the results.

   CAREFUL:
   The arguments for the perl function are encoded into a single
   string following the protocol of the CODEC class for array elements,
   i.e. the arguments are separated by the column separator (= (char) 3),
   which is located in front of every argument.

   @param function name of the Perl function
   @param inargs number of arguments following this parameter
   @param ... variable number of arguments to pass to the Perl function

   @return a C2PERLResult_t result set structure
 */
C2PERLResult_t *C2PERLCall(const char *function, const int inargs, ...)
{
  char joinedargs[65535];
  char addargs[4096];
  char *pjoinedargs=joinedargs;
  char *args[] = { joinedargs, NULL};
  va_list ap;
  char *s;
  int i;

  if (!g_init)
    {
      printf ("Error: C2PERL not initialized\nPlease, do C2PERLInit\n");
      return 0;
    }

  if (!function) {
      printf ("Error: you have to provide a function to be called\n");
      return 0;
  }
  va_start(ap, inargs);
  for (i=0; i< inargs; i++) {
    int j;
      s = va_arg(ap, char *);
      /*for(j=0;j<strlen(s);j++) {
	printf("DEBUG: inarg %d.%d: =>%c (%d)<\n",i,j,s[j],(int) s[j]);
	}*/
      if(s[0]!=3)
	sprintf(addargs,"%c%s",3,s);
      else
	sprintf(addargs,"%s",s);

      memcpy(pjoinedargs,addargs,strlen(addargs));
      pjoinedargs+=(strlen(addargs));
  }
  *pjoinedargs=0;
  va_end(ap);
  if (getenv("DEBUG_C2PERL")) {
    printf("C2PERL: Strlen is %d\n",strlen(joinedargs));
  }
  return ExecuteFunctionResult(args, function);
}

///////////////////////////////////////////////////////////////////////////
C2PERLResult_t *C2PERLCallCodedArg(const char *function, const char *arg, unsigned int length)
///////////////////////////////////////////////////////////////////////////
{
  char joinedargs[65535];
  char addargs[4096];
  char *pjoinedargs=joinedargs;
  char *args[] = { joinedargs, NULL};
  char *s;
  int i;

  if (!g_init)
    {
      printf ("Error: C2PERL not initialized\nPlease, do C2PERLInit\n");
      return 0;
    }

  if (!function) {
      printf ("Error: you have to provide a function to be called\n");
      return 0;
  }

  if (length > 65534) {
    printf ("Error: C2PERL exceeded length of input argument [65534]\n");
    return 0;
  }
   
  memcpy(joinedargs,arg,length);
  *(joinedargs+length) = 0;

  if (getenv("DEBUG_C2PERL")) {
    printf("C2PERL: Strlen is %d\n",strlen(joinedargs));
  }
  return ExecuteFunctionResult(args, function);
}

/**
   Executes a Perl function in ARRAY context

   @param args arguments to pass to the Perl function in form of
          a NULL terminated list of C strings
   @param functionName name of the Perl function to call

   @return a C2PERLResult_t result set structure
 */
C2PERLResult_t *ExecuteFunctionResult(char **args, const char *functionName)
{
  dSP;
  int i;
  C2PERLResult_t *result;

  if (!g_init)
    {
      printf ("Error: not connected to C2PERL\nPlease, do C2PERLConnect\n");
      return 0;
    }

  result=(C2PERLResult_t*)malloc (sizeof(C2PERLResult_t));
  if (!result)
    {
      printf ("Error asking for memory in %s\n", functionName);
      return NULL;
    }

  // let perl remember the status of the stack (+ tmp variables) before
  // we issue a call
  ENTER ;
  SAVETMPS ;

  result->result_count=call_argv(functionName, G_ARRAY  , args);
  SPAGAIN ;

  result->current=0;
  result->results=NULL;

  if (getenv("DEBUG_C2PERL")) {
    printf("C2PERL: no. of results: %d\n", result->result_count);
  }
  if (result->result_count)
    {
      result->results=(char**)malloc((result->result_count)*sizeof (char*));
      if (!result->results)
	{
	  printf ("Error asking for memory in %s\n", functionName);
	  free (result);
	  return 0;
	}
    }



  // copy results away, so that perl can correctly free the memory
  for (i=0; i<result->result_count; i++) {
    char* tptr;
    tptr = POPp;
    if((result->results[i]=malloc(strlen(tptr)+1))==NULL) {
      fprintf(stderr,"Error: Could not allocate memory (result->results[])\n");
    }
    strcpy(result->results[i],tptr);
    if (getenv("DEBUG_C2PERL")) {
      printf("---C2Perl: result(%d)---\n%s\n---C2Perl: result(%d)---\n",i,result->results[i],i);
    }
  }

  // local stack pointer to global stack pointer
  PUTBACK ;

  // let perl free its temporary variables
  FREETMPS ;
  LEAVE ;

  return result;

}


/**
   Executes the Perl "Execute" function in SCALAR context.
   TODO: This function seems to be obsolete. Check and clean it out.
         The functionName argument is never used.

   @param args arguments to pass to the Perl function in form of
          a NULL terminated list of C strings
   @param functionName (passed in but never used)

   @return a C2PERLResult_t result set structure
 */
int ExecuteFunctionScalar(char **args, const char *functionName)
{
  dSP ;
  int error;
  int n;
  
  if (!g_init)
    {
      printf ("Error: not connected to C2PERL\nPlease, do C2PERLConnect\n");
      return 0;
    }

  
  n = perl_call_argv("Execute", G_SCALAR  , args);

  SPAGAIN;

  error =  ( n ?  (POPi) : -1);

  return error;
}

/**
  Gets next result from a result set.
  Note: a pointer into an existing C2PERLResult_t is returned. The
  Caller must NOT free the pointer.

  @param res a C2PERLResult_t result set structure
  
  @return pointer to result entry, NULL if no more results exist 
*/
const char *C2PERLFetchResult(C2PERLResult_t *res)
{
  if (!res) 
    {
      printf ("Error: not enough arguments in C2PERLFetchResult\n");
      return NULL;
    }
  
  res->current++;
  if (res->current>res->result_count)
    return NULL;
  
  return res->results[res->current-1];
}

/**
   Resets the result set iterator, so that a next C2PERLFetchResult()
   returns the first result.

   @param res a C2PERLResult_t result set structure
*/
void C2PERLResetResult(C2PERLResult_t *res)
{
   if (!res) 
     printf ("Error: not enough arguments in C2PERLResetResult\n");
   else
     res->current=0;
}

/**
   Frees a C2PERLResult_t object.

   @param res a C2PERLResult_t result set structure

   @return 0 for success, -1 in case of error.
*/
int C2PERLFreeResult(C2PERLResult_t *res)
{
  int i;

  if (!res) 
    fprintf (stderr,"Error: not enough arguments in C2PERLResetResult\n");
  else
    {
	if (res->results) {
	  for(i=0;i<res->result_count;i++)
	    free(res->results[i]);
	  free(res->results);
	}
	free(res);
    }
  return 0;
}

