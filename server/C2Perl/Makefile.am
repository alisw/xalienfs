## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = foreign 1.4

## define that we want building in SUBDIRS. To prevent automake
## to build the subdirs first, the '.' needs to be included
SUBDIRS = . 

noinst_LTLIBRARIES = libUI-C2PERL.la
libUI_C2PERL_la_SOURCES = C2PERL.c C2PERL.h
include_HEADERS = C2PERL.h

## the libraries and linker flags for perlembed.
LDADD=$(PERL_LIBS)
LDFLAGS=$(PERL_LDOPTS)

## INCLUDES can be used to pass other cpp options apart from -I
INCLUDES= $(PERL_CCOPTS)

