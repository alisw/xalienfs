#ifndef C2PERL_H
#define C2PERL_H

/*
 * C2PERL C-PERL Binding
 *
 * Andreas-Joachim Peters 04/2004
 */

#ifdef __cplusplus
extern "C" {
#endif
    
    /* the initialization state of the perl module */
extern int g_init;
    
/**************************************************************************
 * C2PERL API data types.
 **************************************************************************/
    
  /** @struct C2PERLResultStruct C2Perl.h
      This structure is used to return the results from the embedded
      Perl calls. The results can be iterated over using the
      C2PERLFetchResult() function.
  */
    typedef struct C2PERLResultStruct {
	char **results;      /**< array of result strings */
	int    result_count; /**< number of results */
	int    current;      /**< current result */
    } C2PERLResult_t;
    
    int C2PERLInit(const char *perlmodule);
    int C2PERLIsInitialized();
    int C2PERLClose();
    int C2PERLPerlDestroy();
    
    C2PERLResult_t *C2PERLCall(const char *function, const int inargs, ...);
    C2PERLResult_t *C2PERLCallCodedArg(const char *function, const char *arg, unsigned int length);

    const char *C2PERLFetchResult(C2PERLResult_t *res);
    void C2PERLResetResult(C2PERLResult_t *res);
    int C2PERLFreeResult(C2PERLResult_t *res);
    
#ifdef C2PERL_INTERNAL
    /* Functions used only inside the AliEn API. */
    C2PERLResult_t *ExecuteFunctionResult(char **args, const char *functionName);
    int ExecuteFunctionScalar(char **args, const char *functionName);
#endif
    
#ifdef __cplusplus
}
#endif

#endif
