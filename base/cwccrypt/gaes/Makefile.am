####### This is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs
AUTOMAKE_OPTIONS = foreign 1.5

noinst_LTLIBRARIES = libgladmanaes.la

libgladmanaes_la_SOURCES = aescrypt.c aes.h aesopt.h \
			   aestab.c aestab.h \
                           aeskey.c

noinst_PROGRAMS = gavtest

gavtest_SOURCES = aesaux.cpp aescpp.h aestst.h aesaux.h aesgav.cpp
gavtest_LDADD = libgladmanaes.la
