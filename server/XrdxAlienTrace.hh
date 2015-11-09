//         $Id: XrdxAlienTrace.hh,v 1.13 2007/10/04 01:34:19 abh Exp $

#ifndef ___XALIENTRACE_H___
#define ___XALIENTRACE_H___

#ifndef NODEBUG

#include <iostream>
#include "XrdxAlienFs/XrdxAlienFs.hh"

#define GTRACE(act)         xAlienFsTrace.What & TRACE_ ## act

#define TRACES(x) \
        {xAlienFsTrace.Beg(epname,tident); cerr <<x; xAlienFsTrace.End();}

#define FTRACE(act, x) \
   if (GTRACE(act)) \
      TRACES(x <<" fn=" << (oh->Name()))

#define XTRACE(act, target, x) \
   if (GTRACE(act)) TRACES(x <<" fn=" <<target)

#define ZTRACE(act, x) if (GTRACE(act)) TRACES(x)

#define DEBUG(x) if (GTRACE(debug)) TRACES(x)

#define EPNAME(x) static const char *epname = x;

#else

#define FTRACE(x, y)
#define GTRACE(x)    0
#define TRACES(x)
#define XTRACE(x, y, a1)
#define YTRACE(x, y, a1, a2, a3, a4, a5)
#define ZTRACE(x, y)
#define DEBUG(x)
#define EPNAME(x)

#endif

// Trace flags
//
#define TRACE_MOST     0x3fcd
#define TRACE_ALL      0x8ffffff
#define TRACE_open     0x0001
#define TRACE_read     0x0002
#define TRACE_close    TRACE_open
#define TRACE_delay    0x0004
#define TRACE_debug    0x0f00
#define TRACE_fsctl    0x0800
#endif
