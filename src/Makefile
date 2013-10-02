CXX=g++

DEFS=-D_MT -DNOPCH -DUSE_SSL 

# Detect MinGW
MINGW=$(shell uname -s|grep -i mingw32)

# Detect Mac OS X
MACOSX=$(shell uname -s|grep -i darwin)

# Link boost statically
DEFS += -DFOURWAYSSE2 

# C++ compiler to use for cryptopp
CXX2=$(CXX)

LIBBOOST_SUFFIX=-mt

ifeq "$(MACOSX)" ""
	LIBS=-Wl,-Bstatic
endif

LIBS= \
   -l boost_system$(LIBBOOST_SUFFIX) \
   -l boost_filesystem$(LIBBOOST_SUFFIX) \
   -l boost_program_options$(LIBBOOST_SUFFIX) \
   -l boost_thread$(LIBBOOST_SUFFIX) \
   -l boost_chrono$(LIBBOOST_SUFFIX) \
   -l db_cxx \
   -l ssl \
   -l crypto

namecoin: USE_UPNP:=1
	ifdef USE_UPNP
		LIBS += -l miniupnpc
		DEFS += -DUSE_UPNP=$(USE_UPNP)
	endif
	ifneq "$(MINGW)" ""
		# MinGW-specific
		DEFS += -D__NO_SYSTEM_INCLUDES -D__WXMSW__ -DFOURWAYSSE2 -DBOOST_THREAD_USE_LIB 
		LIBS += -lws2_32 -lshlwapi -lmswsock -lole32 -loleaut32 -luuid -lgdi32
		LIBBOOST_SUFFIX=-mgw46-mt-1_50
	else
                ifneq "$(MACOSX)" ""
                	# Mac OS X specific
			DEFS += -DMAC_OSX -DMSG_NOSIGNAL=SO_NOSIGPIPE -Wno-deprecated
			CXX2=gcc-4.2
		else
			DEFS+=-DBOOST_THREAD_USE_LIB 
			LIBS+= -Wl,-Bdynamic
		endif
                
		# Linux-specific
		LIBS+= \
			-l gthread-2.0 \
			-l z \
			-l dl \
			-l pthread
	endif

namecoind: USE_UPNP:=0
	ifdef USE_UPNP
		LIBS += -l miniupnpc
		DEFS += -DUSE_UPNP=$(USE_UPNP)
	endif
	ifneq "$(MINGW)" ""
		# MinGW-specific
		DEFS += -D__NO_SYSTEM_INCLUDES -D__WXMSW__ -DBOOST_THREAD_USE_LIB 
		LIBS += -l mingwthrd -lws2_32 -lshlwapi -lmswsock -lole32 -loleaut32 -luuid -lgdi32
	else
                ifneq "$(MACOSX)" ""
                	# Mac OS X specific
			DEFS += -DMAC_OSX -DMSG_NOSIGNAL=SO_NOSIGPIPE -Wno-deprecated
			CXX2=gcc-4.2
		else
			DEFS += -DBOOST_THREAD_USE_LIB 
			LIBS+= -Wl,-Bdynamic
		endif
        
		# Linux-specific
		LIBS+= \
			-l gthread-2.0 \
			-l z \
			-l dl \
			-l pthread
	endif

CXXFLAGS=-O2 -Wno-invalid-offsetof -Wformat $(DEFS) $(INCLUDEPATHS)
HEADERS=headers.h strlcpy.h serialize.h uint256.h util.h key.h bignum.h base58.h \
    script.h allocators.h db.h walletdb.h crypter.h net.h irc.h keystore.h main.h wallet.h bitcoinrpc.h uibase.h ui.h noui.h init.h auxpow.h

OBJS= \
    obj/auxpow.o \
    obj/util.o \
    obj/key.o \
    obj/script.o \
    obj/db.o \
    obj/walletdb.o \
    obj/crypter.o \
    obj/net.o \
    obj/irc.o \
    obj/keystore.o \
    obj/main.o \
    obj/wallet.o \
    obj/bitcoinrpc.o \
    obj/init.o \
    cryptopp/obj/sha.o \
    cryptopp/obj/cpu.o

all: namecoind


obj/%.o: %.cpp $(HEADERS)
	$(CXX) -c $(CXXFLAGS) -DGUI -o $@ $<

cryptopp/obj/%.o: cryptopp/%.cpp
	$(CXX2) -c $(CXXFLAGS) -O3 -o $@ $<

obj/nogui/%.o: %.cpp $(HEADERS)
	$(CXX) -c $(CXXFLAGS) -o $@ $<

obj/test/%.o: test/%.cpp $(HEADERS)
	$(CXX) -c $(CFLAGS) -o $@ $<

obj/nogui/namecoin.o: namecoin.h

namecoind: $(OBJS:obj/%=obj/nogui/%) obj/nogui/namecoin.o
	$(CXX) $(CXXFLAGS) -o $@ $(LIBPATHS) $^ $(LIBS)

clean:
	-rm -f namecoin namecoind
	-rm -f obj/*.o
	-rm -f obj/nogui/*.o
	-rm -f obj/test/*.o
	-rm -f cryptopp/obj/*.o
	-rm -f headers.h.gch
