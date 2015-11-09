#define CODEC_CXX
#include "CODEC.h"

#include "config.h"
#define DEBUG_TAG "CODEC"
#include "debugmacros.h"


static int ncolumn = 0;
static int nfield  = 0;

TBytestreamCODEC::TBytestreamCODEC() {
  //DEBUGMSG(5,<< "Constructor TBytestreamCODEC\n");
  fDecoded=false;
  strcpy(fFieldname,"");
  strcpy(fField,"");
  fEncodebuffer=NULL;
  fStreamField=0;
  fStreamArray=0;
}

/**
   returns a pointer to the field value specified through the
   stream identifier, column index and field index

   @param stream stream identifier
   @param column column index
   @param field field index

   @return char pointer to the field's value
*/
const char* TBytestreamCODEC::GetFieldValue(int stream, int column, int field)
  const {
  DEBUGMSG(8,<< "GetFieldValue:" << COLUMN_ADDRESS(stream,column,field,kFieldvalue) << "): >\n");
  return fStreamArray[COLUMN_ADDRESS(stream,column,field,kFieldvalue)];
}


/**
   returns a pointer to the field name specified through the
   stream identifier, column index and field index

   @param stream stream identifier
   @param column column index
   @param field field index

   @return char pointer to the field's name
*/
const char* TBytestreamCODEC::GetFieldName(int stream, int column, int field)
  const {
    DEBUGMSG(8,<< "GetFieldName:" << COLUMN_ADDRESS(stream,column,field,kFieldname) << "): >\n");
  return fStreamArray[COLUMN_ADDRESS(stream,column,field,kFieldname)];
}

/**
   seems obsolete
   TODO: remove if obsolete
*/
const char* TBytestreamCODEC::GetField(int stream, int column, const char* fieldname) const {
  return NULL;
}

/**
   @return int - number of columns in stream <stream>
*/

int TBytestreamCODEC::GetStreamColumns(int stream) const {
  
  DEBUGMSG(1,<< "GetStreamColumns: " << fStreamColumns[stream] << "\n");
  
  return ( (stream<MAXSTREAM)&&(stream>=0)?fStreamColumns[stream]:-1);
}

/**
     Returns number of fields of a certain type found in the specified
     column of the stream.
     
     @param stream stream identifier
     @param column column identifier
     @param type field type identifier
     
     @return number of fields found
*/
int TBytestreamCODEC::GetStreamField(int stream, int column, int type) const {
  DEBUGMSG(1,"GetStreamField: " << fStreamField[FIELD_ADDRESS(stream,column,type)] << "\n";);
       return ( ((stream<MAXSTREAM)&&(stream>=0)&&(column<MAXCOLUMN)&&(column>=0)&&(type>=kFieldname)&&(type<=kFieldvalue))?fStreamField[FIELD_ADDRESS(stream,column,type)]:-1);
     }


/**
   Decodes the passed in buffer.

   Note: The decoding modifies the buffer! The field separators are replaced
   by '\0' while an index to the fields is built
*/
bool TBytestreamCODEC::Decode(char* codedBuffer, unsigned int size) {

  DEBUGMSG(8,<< "codedBuffer (length:" << size << "): >"
	   << codedBuffer << "<\n");

  unsigned int lsize=size;
  int lStream = -1;
  int lColumn = -1;
  int lField = -1;
  int maxlField=-1;
  int maxlColumn=-1;
  int lFieldname = -1;

  if (size > MAXBUFFER) {
    lsize = MAXBUFFER;
    Warn("Codedbuffer size overflow!");
  }
  
  fStreamColumns[kSTDOUT]=fStreamColumns[kSTDERR]
    =fStreamColumns[kOUTPUT]=fStreamColumns[kENVIR]=0;

  char* ptr = (char*)codedBuffer;

  // new loop to determine size of the requested decoding array (columns + fields)

 
  for (unsigned int i=0; i< lsize; i++) {
    if (((*ptr) <=kOutputterminator)&&((*ptr)>=kStreamEnd)) {
      // we found some separator/terminator
      switch (*ptr) {
      case kStreamEnd:
	DEBUGMSG(8,<< "found kStreamEnd at " << i << "\n");
	break;
      case kFieldseparator:
	DEBUGMSG(8,<< "found kFieldseparator at " << i << "\n");
	lField++;
	if (lField>maxlField) 
	  maxlField = lField;
	if (lField >= MAXFIELD) {
	  Error("Exceeded maximum number of fields!");
	  return false;
	}
	break;
      case kFielddescriptor:
	DEBUGMSG(8,<< "found kFielddescriptor at " << i << "\n");
	lFieldname++;
	if (lFieldname >= MAXFIELD) {
	  Error("Exceeded maximum number of field descriptors!");
	  return false;
	}
	break;
      case kColumnseparator:
	DEBUGMSG(8,<< "found kColumnseparator at " << i <<"\n");
	lColumn++;
	if (lColumn>maxlColumn) 
	  maxlColumn = lColumn;
	lField = 0;
	lFieldname = 0;

	if (lColumn >= MAXCOLUMN) {
	  Error("Exceeded maximum number of columns!");
	  return false;
	}
	break;
      case kStdoutindicator:
	DEBUGMSG(8,<< "found kStdoutindicator at " << i << "\n");
	lStream = kSTDOUT;
	lColumn = -1;
	lField = -1;
	lFieldname = -1;
	break;
      case kStderrindicator:
	DEBUGMSG(8,<< "found kStderrindicator at " << i << "\n");
	lStream = kSTDERR;
	lColumn = -1;
	lField = -1;
	lFieldname = -1;
	break;
      case kOutputindicator:
	DEBUGMSG(8,<< "found kOutputindicator at " << i << "\n");
	lStream = kOUTPUT;
	lColumn = -1;
	lField = -1;
	lFieldname = -1;
	break;
      case kOutputterminator:
	// now used for start of Environment hash
	DEBUGMSG(8,<< "found kOutputterminator at " << i << "\n");
	lStream = kENVIR;
	lColumn = -1;
	lField = -1;
	lFieldname = -1;
	break;
      default:
	Fatal("Programm is messed up!");
      }
    }
    ptr++;
  }

  // recheck the allocated size of the output array
  if ( (! fStreamArray) || (! fStreamField) || ((maxlColumn+1) > ncolumn ) || ((maxlField+1) > nfield) || (ncolumn > DEFAULTCOLUMN) || (nfield > DEFAULTFIELD) ) {
    // reallocate/allocate
    DEBUGMSG(1,<< "in: reallocate" << ncolumn << "/" << maxlColumn << ":" << nfield << "/" << maxlField  << "\n");
    if (maxlField < DEFAULTFIELD)
      nfield = DEFAULTFIELD;
    else 
      nfield = maxlField + 1;

   
    if (maxlColumn < DEFAULTCOLUMN)
	ncolumn = DEFAULTCOLUMN;
    else 
      ncolumn = maxlColumn + 1;

    int streamsize = sizeof(char*) *MAXSTREAM*(ncolumn)*nfield*2;
    int fieldsize  = sizeof(int)*MAXSTREAM*ncolumn*2;

    DEBUGMSG(1,<< "out: reallocate" << ncolumn << "/" << lColumn << ":" << nfield << "/" << maxlField << ":=" << streamsize << ":" << fieldsize << "\n");
    if (fStreamArray) 
      fStreamArray = (char**)realloc(fStreamArray,streamsize);
    else 
      fStreamArray = (char**)malloc(streamsize);

    if (fStreamField)
      fStreamField = (int*)realloc(fStreamField, fieldsize);
    else 
      fStreamField = (int*)malloc(fieldsize);

  } else {
    DEBUGMSG(1,<< "no-allocate" << ncolumn << "/" << lColumn << ":" << nfield << "/" << maxlField<< "\n");
  }

  // now decode
  lStream = -1;
  lColumn = -1;
  lField = -1;
  lFieldname = -1;

  ptr = (char*)codedBuffer;
  for (unsigned int i=0; i< lsize; i++) {
    if (((*ptr) <=kOutputterminator)&&((*ptr)>=kStreamEnd)) {
      // we found some separator/terminator
      switch (*ptr) {
      case kStreamEnd:
	DEBUGMSG(8,<< "found kStreamEnd at " << i << "\n");
	break;
      case kFieldseparator:
	DEBUGMSG(8,<< "found kFieldseparator at " << i << "\n");
	*ptr=0;

	DEBUGMSG(8,<< "SetFieldValue Array:" << COLUMN_ADDRESS(lStream,lColumn,lField,kFieldvalue) << "): >\n");
	fStreamArray[COLUMN_ADDRESS(lStream,lColumn,lField,kFieldvalue)] = (ptr+1);
	//	fStreamArray[lStream][lColumn][lField][kFieldvalue] = (ptr+1);
	DEBUGMSG(8,<< "SetFieldValue Field:" << FIELD_ADDRESS(lStream,lColumn,kFieldvalue) << "): >\n");
	fStreamField[FIELD_ADDRESS(lStream,lColumn,kFieldvalue)]++;
	//	fStreamField[lStream][lColumn][kFieldvalue]++;
	lField++;
	if (lField >= MAXFIELD) {
	  Error("Exceeded maximum number of fields!");
	  return false;
	}
	break;
      case kFielddescriptor:
	DEBUGMSG(8,<< "found kFielddescriptor at " << i << "\n");
	*ptr=0;
	fStreamArray[COLUMN_ADDRESS(lStream,lColumn,lField,kFieldname)] = (ptr+1);
	//	fStreamArray[lStream][lColumn][lField][kFieldname] = (ptr+1);
	fStreamField[FIELD_ADDRESS(lStream,lColumn,kFieldname)]++;
	//	fStreamField[lStream][lColumn][kFieldname]++;
	lFieldname++;
	if (lFieldname >= MAXFIELD) {
	  Error("Exceeded maximum number of field descriptors!");
	  return false;
	}
	break;
      case kColumnseparator:
	DEBUGMSG(8,<< "found kColumnseparator at " << i << "stream: " << lStream << "\n");
	*ptr=0;
	lColumn++;
	lField = 0;
	lFieldname = 0;
	// fix for the returned answer from the client to the api server: the environment hash is sent without any other stream, so lstream would be negative and creates a SEGV!
	if (lStream > -1) {
	  fStreamField[FIELD_ADDRESS(lStream,lColumn,kFieldname)]=0;
	  fStreamField[FIELD_ADDRESS(lStream,lColumn,kFieldvalue)]=0;
	  //	fStreamField[lStream][lColumn][kFieldname]=0;
	  //	fStreamField[lStream][lColumn][kFieldvalue]=0;
	  fStreamColumns[lStream]++;
	}

	if (lColumn >= MAXCOLUMN) {
	  Error("Exceeded maximum number of columns!");
	  return false;
	}
	break;
      case kStdoutindicator:
	DEBUGMSG(8,<< "found kStdoutindicator at " << i << "\n");
	*ptr=0;
	lStream = kSTDOUT;
	lColumn = -1;
	lField = -1;
	lFieldname = -1;
	break;
      case kStderrindicator:
	DEBUGMSG(8,<< "found kStderrindicator at " << i << "\n");
	*ptr=0;
	lStream = kSTDERR;
	lColumn = -1;
	lField = -1;
	lFieldname = -1;
	break;
      case kOutputindicator:
	DEBUGMSG(8,<< "found kOutputindicator at " << i << "\n");
	*ptr=0;
	lStream = kOUTPUT;
	lColumn = -1;
	lField = -1;
	lFieldname = -1;
	break;
      case kOutputterminator:
	// now used for start of Environment hash
	DEBUGMSG(8,<< "found kOutputterminator at " << i << "\n");
	*ptr=0;
	lStream = kENVIR;
	lColumn = -1;
	lField = -1;
	lFieldname = -1;
	break;
      default:
	Fatal("Programm is messed up!");
      }
    }
    ptr++;
  }

  DEBUGMSG(8, << "Codec Decode successful\n");
  fDecoded=true;
  return true;
}

/**
   Seems to be obsolete. Only used in a test
   TODO: get rid of this member function if it is really obsolete
*/
char* TBytestreamCODEC::EncodeInput(const int inargs, ...) {
  va_list vparams;
  va_start(vparams, inargs);


  char **list = new char* [inargs];
  for (int i=0; i<inargs; ++i) {
    list[i]=va_arg(vparams, char*);
  }

  fEncodebuffer = EncodeInputArray(inargs,list);
  delete[] list;

  return fEncodebuffer;
}

/**
   Returns a pointer to an encoded C string with all the fields of the input
   C string array separated by kFieldseparators.
   The returned pointer must NOT be freed!

   The encoded string follows this pattern:
   kColumnseparator
             kFieldseparator value1
             kFieldseparator value2

   @param inargs size of args array
   @param args array of C strings

   @return C string of encoded array elements
 */
char* TBytestreamCODEC::EncodeInputArray(const int inargs,char *args[]) {
  int buflen=0;

  for (int i=0; i<inargs; ++i) {
    buflen += strlen(args[i]) + 1;
  }
  buflen++;

  char *fEncodebuffer = new char [buflen];

  char *ptr = fEncodebuffer;
  for (int i=0; i<inargs; ++i) {
    DEBUGMSG(8,<< "args[" << i << "]: >" << args[i] << "<\n");
    *ptr=kColumnseparator;
    ptr++;
    strcpy(ptr,args[i]);
    //    printf("Putting |%s|\n",args[i]);
    ptr += strlen(args[i]);
  }
  *ptr='\0';

  //  printf("Returning args |%d| as |%s|\n",inargs,fEncodebuffer);
  return fEncodebuffer;
}

/**
   returns a string containing the encoded keys and values of the
   passed <string,string> map.

   The encoded string follows this pattern:
             kFielddescriptor keyname1 kFieldseparator value1
             kFielddescriptor keyname2 kFieldseparator value2

   @param hash <string,string> map containing key value pairs

   @return encoded string of the map's elements
*/
const std::string TBytestreamCODEC::EncodeInputHash(std::map<std::string, std::string> &hash) const {
  typedef std::map<std::string, std::string>::const_iterator iter;
  std::stringstream res;

  res << (char) kColumnseparator;
  for(iter i=hash.begin();i != hash.end(); ++i) {
    res << (char) kFielddescriptor << i->first
	<< (char) kFieldseparator << i->second;
  }

  DEBUGMSG(8,<< "encoded hash: >" << res.str() << "<\n"); 
  //DEBUGHEXDUMP(8,"encoded hash:",res.str().c_str(),strlen(res.str().c_str()));
  return res.str();
}

/**
   Debug function to dump the contents of the result codec
 */
void TBytestreamCODEC::DumpResult() const {
  std::string streamname[4] = { "stdout","stderr","result_structure",
			   "misc_hash"};
  for (int u=0; u<= kENVIR;u++) {
    std::cout << "===============>Stream " << streamname[u] << "\n";
    // loop over the columns
    //DEBUGMSG(10,<< "columns: " << GetStreamColumns(u) << "\n");
    for (int x=0; x < GetStreamColumns(u); x++) {
      std::cout << "  [Col " << x << "]:";

      // loop over the Streamfields
      //      DEBUGMSG(10,<< " fieldnames: " << 
      //	       GetStreamField(u,x,kFieldname)
      //	       << "\n");
      //DEBUGMSG(10,<< " fieldvalues: " << 
      //	       GetStreamField(u,x,kFieldvalue)
      //	       << "\n");
      int nval= GetStreamField(u,x,kFieldname);
      for (int y=0; y <
	     GetStreamField(u,x,kFieldvalue);y++) {
	std::cout << "    ";
	if(y<nval)
	  std::cout << "[Tag: " << GetFieldName(u,x,y) << "] => ";

	std::cout << ">" << GetFieldValue(u,x,y) << "<\n";
      }

    }
    std::cout << "\n";
  }    
}
