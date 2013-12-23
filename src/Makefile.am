include Makefile.include

AM_CPPFLAGS += -I$(builddir)

noinst_LIBRARIES = libnamecoin_server.a libnamecoin_common.a
if ENABLE_WALLET
noinst_LIBRARIES += libnamecoin_wallet.a
endif

bin_PROGRAMS = $(NAMECOIND)
EXTRA_PROGRAMS = namecoind

SUBDIRS = . $(BUILD_QT) $(BUILD_TEST)
DIST_SUBDIRS = . qt test
.PHONY: FORCE
# namecoin core #
NAMECOIN_CORE_H = allocators.h base58.h bignum.h \
  crypter.h db.h init.h \
  key.h keystore.h main.h \
  net.h noui.h script.h serialize.h \
  uint256.h util.h walletdb.h wallet.h

JSON_H = json/json_spirit.h json/json_spirit_error_position.h \
  json/json_spirit_reader.h json/json_spirit_reader_template.h \
  json/json_spirit_stream_reader.h json/json_spirit_utils.h \
  json/json_spirit_value.h json/json_spirit_writer.h \
  json/json_spirit_writer_template.h

obj/build.h: FORCE
	@$(MKDIR_P) $(abs_top_builddir)/src/obj
	@$(top_srcdir)/share/genbuild.sh $(abs_top_builddir)/src/obj/build.h \
	  $(abs_top_srcdir)
version.o: obj/build.h

libnamecoin_server_a_SOURCES = \
  keystore.cpp \
  main.cpp \
  net.cpp \
  $(JSON_H) \
  $(NAMECOIN_CORE_H)

libnamecoin_wallet_a_SOURCES = \
  db.cpp \
  crypter.cpp \
  wallet.cpp \
  walletdb.cpp \
  $(NAMECOIN_CORE_H)

libnamecoin_common_a_SOURCES = \
  init.cpp \
  irc.cpp \
  namecoin.cpp \
  auxpow.cpp \
  bitcoinrpc.cpp \
  key.cpp \
  script.cpp \
  util.cpp \
  cryptopp/sha.cpp \
  cryptopp/cpu.cpp \
  $(NAMECOIN_CORE_H)

nodist_libnamecoin_common_a_SOURCES = $(top_srcdir)/src/obj/build.h
#

# namecoind binary #
namecoind_LDADD = libnamecoin_server.a libnamecoin_common.a $(BOOST_LIBS) $(QT_LIBS)
if ENABLE_WALLET
namecoind_LDADD += libnamecoin_wallet.a
endif
# 
namecoind_SOURCES = namecoind.cpp
#

if TARGET_WINDOWS
namecoind_SOURCES += namecoind-res.rc
endif

AM_CPPFLAGS += $(BDB_CPPFLAGS)
namecoind_LDADD += $(BDB_LIBS)

qt/namecoinstrings.cpp: $(libnamecoin_server_a_SOURCES) $(libnamecoin_common_a_SOURCES)
	@test -n $(XGETTEXT) || echo "xgettext is required for updating translations"
	@cd $(top_srcdir); XGETTEXT=$(XGETTEXT) share/qt/extract_strings_qt.py

CLEANFILES = leveldb/libleveldb.a leveldb/libmemenv.a *.gcda *.gcno

DISTCLEANFILES = obj/build.h

EXTRA_DIST = leveldb Makefile.include

clean-local:
	-$(MAKE) -C leveldb clean
	rm -f leveldb/*/*.gcno leveldb/helpers/memenv/*.gcno
