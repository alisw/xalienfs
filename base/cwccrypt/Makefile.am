####### This is the input file for automake, which will generate Makefile.in ##########
## not a GNU package. You can remove this line, if
## you have all needed files, that a GNU package needs

AUTOMAKE_OPTIONS = foreign
SUBDIRS = gaes . 

noinst_LTLIBRARIES = libcwccrypt.la

libcwccrypt_la_SOURCES = spc_cwc.c \
                         spc_cwc.h \
                         spc_cypherq.c \
                         spc_cypherq.h \
                         spc_b64.c \
                         spc_b64.h \
                         spc_helpers.c\
	                 spc_helpers.h

if OPENSSL_AES
INCLUDES = $(OPENSSLINC)
libcwccrypt_la_LIBADD = $(OPENSSLLIB) $(LIBSSL_LIBCRYPTO)
else 
libcwccrypt_la_LIBADD = gaes/libgladmanaes.la
endif

CFLAGS += -D_GNU_SOURCE

