# -*- fill-column: 56 -*-
AUTOMAKE_OPTIONS =		subdir-objects
ACLOCAL_AMFLAGS =		-I build-aux/m4

nmc_source_dir =		$(top_srcdir)/src
nmc_include_dir =		$(top_srcdir)/src

common_includes =		-I$(nmc_include_dir)		\
				$(DEPS_CFLAGS)

## COMMON FLAGS ###

AM_CXXFLAGS = 			$(common_includes)		\
				-O2 				\
				-Wno-invalid-offsetof		\
				-Wformat			\
				-DNOPCH -DFOURWAYSSE2 -DUSE_SSL	\
				-DUSE_UPNP=0

AM_LDFLAGS = 			$(DEPS_LDFLAGS)

DEPS_LIBS= 	-lminiupnpc	\
		-lboost_system \
		-lboost_filesystem \
		-lboost_program_options \
		-lboost_thread \
		-ldb_cxx \
		-lssl \
		-lcrypto \
		-lgthread-2.0 \
		-lz \
		-ldl \
		-lpthread

## WARNING FLAGS ###

if WARNINGS
AM_CXXFLAGS +=			-Waddress			\
				-Woverloaded-virtual		\
				-Wshadow			\
				-Wconversion			\
				-g
endif

## RELEASE FLAGS ###

if RELEASE
AM_CXXFLAGS +=			-Wall				\
				-D_FORTIFY_SOURCE=2		\
				-fstack-protector-all		\
				-fstack-check			\
				-Wstack-protector
endif

####################################################################

namecoind_sources=	src/auxpow.cpp		\
			src/util.cpp		\
			src/script.cpp		\
			src/db.cpp		\
			src/net.cpp		\
			src/irc.cpp		\
			src/keystore.cpp	\
			src/main.cpp		\
			src/wallet.cpp		\
			src/rpc.cpp		\
			src/init.cpp		\
			src/cryptopp/sha.cpp	\
			src/cryptopp/cpu.cpp	\
			src/namecoin.cpp

namecoind_headers=	src/headers.h		\
			src/strlcpy.h		\
			src/serialize.h		\
			src/uint256.h		\
			src/util.h		\
			src/key.h		\
			src/bignum.h		\
			src/base58.h		\
			src/script.h		\
			src/db.h		\
			src/net.h		\
			src/irc.h		\
			src/keystore.h		\
			src/main.h		\
			src/wallet.h		\
			src/rpc.h		\
			src/uibase.h		\
			src/ui.h		\
			src/noui.h		\
			src/init.h		\
			src/auxpow.h

#################

bin_PROGRAMS		=	namecoind

namecoind_SOURCES	=	$(namecoind_sources) $(namecoind_headers)
namecoind_CPPFLAGS	=	-I$(nmc_include_dir) -I$(nmc_include_dir)/cryptopp $(AM_CXXFLAGS)
namecoind_LDADD		=	-L/usr/lib $(DEPS_LIBS)
#namecoind_LDFLAGS	=	-Wl, -Bdynamic
#namecoind_DEPENDENCIES	=	

#####################

# for boost 1.37, add -mt to the boost libraries

