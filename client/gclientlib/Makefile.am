lib_LTLIBRARIES = libgapiUI.la
noinst_LTLIBRARIES = libgclient.la

include_HEADERS = gapiUI.h gapi_dir_operations.h  gapi_file_operations.h  \
	          gapi_job_operations.h gapi_stat.h  gapi_attr.h



libgapiUI_la_SOURCES = gapiUI.cxx gapiUI.h 
libgapiUI_la_LIBADD = libgclient.la $(top_srcdir)/base/CODEC/libUI-CODEC.la \
         $(top_srcdir)/base/cwccrypt/libcwccrypt.la \
	$(top_srcdir)/base/ShmAuth/libUI-ShmAuth.la \
	$(OPENSSLLIB) $(LIBSSL_LIBCRYPTO) $(SHELL_LIBS) -lpthread 
libgapiUI_la_LDFLAGS = -shrext ".so" -DWITH_OPENSSL -version-info 4:0:0


if ENABLE_PERLMODULE
INCLUDES = -I$(top_srcdir)/base -I$(top_srcdir)/server \
           -I$(top_srcdir)/base/cwccrypt $(GAPIINC) \
           -I$(SWIG_INC)/ -I$(SWIG_INC)/perl5/ \
	   $(GLOBUS_INCLUDE) $(OPENSSLINC) -I$(XROOTD_LOCATION)/include/xrootd/ -I$(XROOTD_LOCATION)/src/ -D_REENTRANT
else
INCLUDES = -I$(top_srcdir)/base -I$(top_srcdir)/server \
           -I$(top_srcdir)/base/cwccrypt $(GAPIINC) \
	   $(GLOBUS_INCLUDE) $(OPENSSLINC) -I$(XROOTD_LOCATION)/include/xrootd/ -I$(XROOTD_LOCATION)/src/ -D_REENTRANT 
endif

libgclient_la_SOURCES = gclient.cxx gclient.h \
			gapi_dir_operations.h gapi_dir_operations.cxx \
			gapi_stat.h gapi_stat.cxx \
	                gapi_file_operations.h gapi_file_operations.cxx \
			gapi_job_operations.h gapi_job_operations.cxx \
			posix_helpers.h posix_helpers.cxx 


libgclient_la_LIBADD = $(SHELL_LIBS) -L$(XROOTD_LOCATION)/lib$(MARK64) -lXrdSec -lXrdClient -lXrdUtils $(OPENSSLLIB) $(LIBSSL_LIBCRYPTO) -ldl

if ENABLE_PERLMODULE

all-local: perlmodule 
install-exec-local: perlmodule-install
uninstall-local: perldmodule-uninstall
clean-local: perlmodule-clean

perlmodule:
	cd perl; ${SWIG} -c++ -perl gapiUI.i
	cd perl; ${PERL} Makefile.PL; make;

perlmodule-install:
	cd perl; make install;


perlmodule-uninstall:
	cd perl; make uninstall;

perlmodule-clean:
	cd perl; rm -f libgapiUI_wrap.so
	cd perl; rm -f gapiUI.pm
	cd perl; rm -f gapiUI_wrap.cxx
	cd perl; rm -f gapiUI_wrap.o

install-exec-local:
	mkdir -p $(DESTDIR)/$(libdir)/../perl/gapiserver
	cp perl/gapi.pm  $(DESTDIR)/$(libdir)/../perl/gapiserver/
endif



