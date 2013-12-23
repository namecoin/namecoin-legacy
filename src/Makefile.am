include Makefile.include

AM_CPPFLAGS += -I$(builddir)

noinst_LIBRARIES = libnamecoin_common.a

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
  db.cpp \
  crypter.cpp \
  wallet.cpp \
  walletdb.cpp \
  keystore.cpp \
  main.cpp \
  net.cpp \
  $(JSON_H) \
  $(NAMECOIN_CORE_H)
#

# namecoind binary #
namecoind_LDADD = libnamecoin_common.a $(BOOST_LIBS) $(QT_LIBS)
# 
namecoind_SOURCES = namecoind.cpp
#

if TARGET_WINDOWS
namecoind_SOURCES += namecoind-res.rc
endif

AM_CPPFLAGS += $(BDB_CPPFLAGS)
namecoind_LDADD += $(BDB_LIBS)

qt/namecoinstrings.cpp: $(libnamecoin_common_a_SOURCES)
	@test -n $(XGETTEXT) || echo "xgettext is required for updating translations"
	@cd $(top_srcdir); XGETTEXT=$(XGETTEXT) share/qt/extract_strings_qt.py

CLEANFILES = *.gcda *.gcno

EXTRA_DIST = Makefile.include
