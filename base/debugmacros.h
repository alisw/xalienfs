// Debug macro
// 2004-08-05 Derek Feichtinger
//
// DEBUGMSG macro: usage DEBUGMSG(int debuglevel,<< msg1 << msg2)
//   prints a message
//
// DEBUGHEXDUMP macro: usage DEBUGHEXDUMP(int debuglevel, << msg1 << msg2,
//                                  ptr,length)
//   prints a message and a hexdump of ptr to ptr+length
//
// output is triggered based on an environment variable. The
// environment variable name and the output are customized based on the
// string value of the DEBUG_TAG macro.
// E.g. if DEBUG_TAG is "foo" then the environment variable will be
// called DEBUG_foo
//
// If the environment variable "DEBUGFILE" is set, the messages will
// be appended to the file given by that environment variable instead
// of stdout.

#  include <iostream>
#ifdef DEBUGMACRO
#  include <iomanip>
#  include <fstream>

#  ifndef DEBUG_TAG
#    define DEBUG_TAG "DEFAULT"
#  endif

#  define DEBUGENVNAME "DEBUG_" DEBUG_TAG 
#  define DEBUGMSG(X,MSG) if(getenv(DEBUGENVNAME)!=NULL) \
                       if(atoi(getenv(DEBUGENVNAME)) >= X ) \
                         if(getenv("DEBUGFILE")!=NULL) {\
                           std::ofstream dbgstr(getenv("DEBUGFILE"),std::ios::app);\
                           dbgstr << "(" << DEBUGENVNAME <<"=" << X \
                                  << "," << __FUNCTION__ << "): " MSG;} \
                         else \
                           std::cout << "(" << DEBUGENVNAME <<"=" << X \
                                  << "," << __FUNCTION__ << "): " MSG;

#  define DEBUGHEXDUMP(X,MSG,PTR,LENGTH) if(getenv(DEBUGENVNAME)!=NULL) \
                       if(atoi(getenv(DEBUGENVNAME)) >= X ) \
                         if(getenv("DEBUGFILE")!=NULL) { \
                           std::ofstream dbgstr(getenv("DEBUGFILE"),std::ios::app);\
                           dbgstr << "(" << DEBUGENVNAME <<"=" << X \
                                  << "," << __FUNCTION__ << "): " MSG;\
                           dbgstr.setf(std::ios::hex); \
                           int counter=0; \
                           for(unsigned char *debgi=(unsigned char*)PTR; \
                               debgi<((unsigned char*) PTR+(LENGTH));debgi++){ \
                              if(++counter%12==1) dbgstr << "\n"; \
			      dbgstr << std::setw(4) \
                                     << "  " << (unsigned int) *debgi; \
                           } \
                           dbgstr.unsetf(std::ios::hex);\
                           dbgstr << "\n"; \
                         } else { \
                           std::cout << "(" << DEBUGENVNAME <<"=" << X \
                                  << "," << __FUNCTION__ << "): " MSG;\
                           std::cout.setf(std::ios::hex); \
                           int counter=0; \
                           for(unsigned char *debgi=(unsigned char*)PTR; \
                             debgi<((unsigned char*) PTR+(LENGTH));debgi++){ \
                              if(++counter%12==1) std::cout << "\n"; \
			      std::cout << std::setw(4) \
                                     << "  " << (unsigned int) *debgi; \
                           }\
                           std::cout.unsetf(std::ios::hex);\
                           std::cout << "\n"; \
                         }
#else
#  define DEBUGMSG(X,MSG)
#  define DEBUGHEXDUMP(X,MSG,PTR,LENGTH)
#endif
// end of DEBUGMSG macro
