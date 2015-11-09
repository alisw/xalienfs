## Thiss is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.5

## define that we want building in SUBDIRS. To prevent automake
## to build the subdirs first, the '.' needs to be included
SUBDIRS = . 

noinst_LTLIBRARIES = libUI-CODEC.la
libUI_CODEC_la_SOURCES = CODEC.cxx CODEC.h
INCLUDES = -I$(top_srcdir)/base
