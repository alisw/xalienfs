#include <config.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <string>
#include <map>
#include <sstream>

#ifndef __CODEC_H
#define __CODEC_H
#define MAXSTREAM 4
#define MAXCOLUMN 1024*64
#define DEFAULTCOLUMN 1024
#ifndef CODEC_CXX
extern int ncolumn;
extern int nfield;
#endif

#define MAXFIELD  128
#define DEFAULTFIELD 16

#define MAXBUFFER 1024*1024*512

//  char* fStreamArray[MAXSTREAM][MAXCOLUMN][MAXFIELD][2]; ///< array containing pointers to the identified fields of the decoded codec string 
#define COLUMN_ADDRESS(a,b,c,d) ( (a * (ncolumn*nfield*2)) + (b *(nfield*2)) + (c * 2) + d)

  //  int   fStreamField[MAXSTREAM][MAXCOLUMN][2]; ///< number of fields in the specified column and stream 
#define FIELD_ADDRESS(a,b,c) ( (a * (ncolumn*2)) + (b * 2) + c )

#ifdef DEBUG
#define DEBUG_TRACE(x) do { printf("DEBUG: %s \t %s\t %s\n",__func__,__PRETTY_FUNCTION__,(x));} while(0)
#else
#define DEBUG_TRACE(x) while (0) {};
#endif

/** @class TBytestreamCODEC TBytestreamCODEC.h

     Class for encoding/decoding structures into/from a single string.
     The different fields are marked using a number of 1 byte separator
     characters.

     SCALAR: passed as 1 element ARRAY

               kColumnseparator
	       kFieldseparator  value

     HASH: passed as 1 element ARRAY

               kColumnseparator
               kFielddescriptor key1   kFieldseparator  value1
               kFielddescriptor key2   kFieldseparator  value2
               ...

     ARRAY (elements can be intermixed scalars and hashes):
               
               kColumnseparator
               kFielddescriptor key1   kFieldseparator  value1
               kFielddescriptor key2   kFieldseparator  value2
	       kColumnseparator
	       kFieldseparator  value
               ...

     NOTE: - "Decoding" modifies the passed in buffer

     @author A. Peters
     @author D. Feichtinger (some modifications and documentation)
     @date 2004-05-12
*/
class TBytestreamCODEC {
 private:
  int fStream; ///< TODO: seems obsolete
  bool fDecoded; ///< TODO: seems not to be used. status flag.

  char* fEncodebuffer; ///< buffer used to store the encoded string
  char fFieldname[1024*8]; ///< TODO: seems obsolete
  char fField[1024*8]; ///< TODO: seems obsolete

  // [stream][column][field][fieldname or field]
  //  char* fStreamArray[MAXSTREAM][MAXCOLUMN][MAXFIELD][2]; ///< array containing pointers to the identified fields of the decoded codec string 
  // doing dynamic now
  char** fStreamArray;
  //  int   fStreamField[MAXSTREAM][MAXCOLUMN][2]; ///< number of fields in the specified column and stream
  int* fStreamField;
 
  int   fStreamColumns[MAXSTREAM];  ///< number of encoded streams

 public:
  enum {kStreamEnd=0, kFieldseparator=1,  kFielddescriptor=2, kColumnseparator=3,
	kStdoutindicator=4, kStderrindicator=5, kOutputindicator=6,
	kOutputterminator=7};  ///< the byte stream indicators


  enum {kSTDOUT = 0, kSTDERR = 1 , kOUTPUT = 2, kENVIR = 3};  ///< stream content types


  enum {kFieldname = 0, kFieldvalue = 1};  // type identifiers for field name and field value

  TBytestreamCODEC();
  ~TBytestreamCODEC() {
    if(fEncodebuffer) delete[] fEncodebuffer;
  }
  
  bool Decode(char* codedBuffer, unsigned int size);

  void DumpResult() const;
  char* EncodeInput(const int inargs,...);
  char* EncodeInputArray(const int inargs,char *args[]);
  const std::string EncodeInputHash(std::map<std::string, std::string> &hash) const;
  const char* GetFieldName(int stream,  int column, int field) const;
  const char* GetFieldValue(int stream,  int column, int field) const;
  const char* GetField(int stream,  int column, const char* fieldname) const;

  /**
     Returns the number of columns found in the specified stream

     @param stream stream identifier

     @return number of columns found
  */
  int         GetStreamColumns(int stream) const ;

  /**
     Returns number of fields of a certain type found in the specified
     column of the stream.

     @param stream stream identifier
     @param column column identifier
     @param type field type identifier

     @return number of fields found
  */
  int         GetStreamField(int stream, int column, int type) const ;

  void Warn(const char* txt) {
    fprintf(stderr,"TBytestreamCODEC [WARN]: %s",txt);
  }
  void Error(const char* txt) {
    fprintf(stderr,"TBytestreamCODEC [ERROR]: %s",txt);
  }
  void Fatal(const char* txt) {
    fprintf(stderr,"TBytestreamCODEC [FATAL]: %s",txt);
    exit(-1);
  }
  
};


#endif

