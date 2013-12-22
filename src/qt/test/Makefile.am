include $(top_srcdir)/src/Makefile.include

AM_CPPFLAGS += -I$(top_srcdir)/src \
  -I$(top_srcdir)/src/qt \
  -I$(top_builddir)/src/qt \
  $(PROTOBUF_CFLAGS) \
    $(QR_CFLAGS)
bin_PROGRAMS = test_namecoin-qt
TESTS = test_namecoin-qt

TEST_QT_MOC_CPP = moc_uritests.cpp moc_paymentservertests.cpp

TEST_QT_H = uritests.h paymentservertests.h paymentrequestdata.h

BUILT_SOURCES = $(TEST_QT_MOC_CPP)

test_namecoin_qt_CPPFLAGS = $(AM_CPPFLAGS) $(QT_INCLUDES) $(QT_TEST_INCLUDES)
test_namecoin_qt_SOURCES = test_main.cpp uritests.cpp paymentservertests.cpp $(TEST_QT_H)
nodist_test_namecoin_qt_SOURCES = $(TEST_QT_MOC_CPP)
test_namecoin_qt_LDADD = $(LIBNAMECOINQT) $(LIBNAMECOIN_SERVER) $(LIBNAMECOIN_WALLET) $(LIBNAMECOIN_CLI) $(LIBNAMECOIN_COMMON) $(LIBLEVELDB) \
  $(LIBMEMENV) $(BOOST_LIBS) $(QT_LIBS) $(QT_DBUS_LIBS) $(QT_TEST_LIBS) \
  $(QR_LIBS) $(PROTOBUF_LIBS) $(BDB_LIBS)

CLEANFILES = $(BUILT_SOURCES) *.gcda *.gcno

