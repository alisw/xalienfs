#######################################################################
## Makefile.am for xAlien FS plugin
##
##
## Version info: $Id: Makefile.am,v 1.5 2006/07/24 07:38:57 apeters Exp $
## Checked in by $Author: apeters $
#######################################################################

if BUILD_SERVER
  SERVER_DIR = server
endif

SUBDIRS	= base client $(SERVER_DIR)

INCLUDES = -I$(XROOTD_LOCATION)/include/xrootd -I$(ALIEN_LOCATION) -O -pg -g 

EXTRA_DIST = configure.ac bootstrap.sh  xrootd-xalienfs.spec base/debugmacros.h acinclude.m4 server/scripts/AlienAS.pl server/scripts/gapiserver.pl server/scripts/gapiservermonitor.pl server/etc/gapiservice.cfg.xml client/clients/share client/gclientlib/perl/gapi.pm client/gclientlib/perl/Makefile.PL client/gclientlib/perl/gapiUI.i client/gclientlib/perl/gapiUI.pm

install-certificates:
	@mkdir -p /tmp/xacert   
	@mkdir -p $(CERTIFICATE_LOCATION)
	@if [ -n `which curl 2>/dev/null` ]; then curl http://pcalienbuild4.cern.ch:8888/x86_64-unknown-linux-gnu/v2-19/download/alien-v2-19-user.tar.bz2 -o /tmp/xacert/alien-v2-19-user.tar.bz2 ; else test -n `which wget 2>/dev/null` && wget http://pcalienbuild4.cern.ch:8888/x86_64-unknown-linux-gnu/v2-19/download/alien-v2-19-user.tar.bz2 -O /tmp/xacert/alien-v2-19-user.tar.bz2; fi
	@cd /tmp/xacert ; bunzip2 alien-v2-19-user.tar.bz2 ; tar xf alien-v2-19-user.tar ; cp -a globus/share/certificates $(CERTIFICATE_LOCATION)/
	@rm -rf /tmp/xacert

rpm:	dist
	cp $(DIST_ARCHIVES) /usr/src/redhat/SOURCES/
	rpmbuild -bb xrootd-xalienfs.spec

