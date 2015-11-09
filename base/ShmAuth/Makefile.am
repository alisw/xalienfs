## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign 1.5

## define that we want building in SUBDIRS. To prevent automake
## to build the subdirs first, the '.' needs to be included

INCLUDES = -I$(top_srcdir)/base $(OPENSSLINC)
LDADD = $(OPENSSLLIB) $(LIBSSL_LIBCRYPTO)

noinst_LTLIBRARIES = libUI-ShmAuth.la
libUI_ShmAuth_la_SOURCES = ShmAuth.h ShmAuth.cxx

