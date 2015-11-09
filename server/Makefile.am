#######################################################################
## Makefile.am for xAlien FS plugin
##
##
## Version info: $Id: Makefile.am,v 1.5 2006/07/24 07:38:57 apeters Exp $
## Checked in by $Author: apeters $
#######################################################################


SUBDIRS	= C2Perl . 

INCLUDES = -I$(top_srcdir)/base -I$(XROOTD_LOCATION)/include/xrootd -I$(XROOTD_LOCATION)/src/ -I$(ALIEN_LOCATION) -O -pg -g 


bin_PROGRAMS = xAlienWorker

xAlienWorker_SOURCES = XrdxAlienWorker.cc

xAlienWorker_LDADD = C2Perl/libUI-C2PERL.la $(PERL_LIBS) -L$(XROOTD_LOCATION)/lib$(MARK64) -lXrdUtils
xAlienWorker_LDFLAGS = -static -export-dynamic $(PERL_LDOPTS)

lib_LTLIBRARIES = libXrdxAlienFs.la

libXrdxAlienFs_la_SOURCES = XrdxAlienFs.cc XrdxAlienFs.hh XrdxAlienTiming.hh XrdxAlienConfig.cc XrdxAlienTrace.hh
libXrdxAlienFs_la_LIBADD = $(top_srcdir)/base/cwccrypt/libcwccrypt.la $(top_srcdir)/base/ShmAuth/libUI-ShmAuth.la $(top_srcdir)/base/CODEC/libUI-CODEC.la -L$(XROOTD_LOCATION)/lib$(MARK64)/  -lXrdUtils -lXrdOfs -lcrypto -luuid -L$(ALIEN_LOCATION)/lib$(MARK64)/
libXrdxAlienFs_la_LDFLAGS = 

install-exec-local:	
			mkdir -p $(DESTDIR)/$(libdir)/../scripts/gapiserver
			cp scripts/AlienAS.pl  scripts/gapiservermonitor.pl  scripts/gapiserver.pl $(DESTDIR)/$(libdir)/../scripts/gapiserver
			cp etc/gapiservice.cfg.xml $(DESTDIR)/$(libdir)/../etc/gapiservice.cfg.xml
