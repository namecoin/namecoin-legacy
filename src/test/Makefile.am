include $(top_srcdir)/src/Makefile.include

AM_CPPFLAGS += -I$(top_srcdir)/src

bin_PROGRAMS = $(TEST_NAMECOIN)
EXTRA_PROGRAMS = test_namecoin

TESTS = test_namecoin

# test_namecoin binary #
test_namecoin_CPPFLAGS = $(AM_CPPFLAGS) $(TESTDEFS)
test_namecoin_LDADD = $(LIBNAMECOIN_COMMON) \
  $(BOOST_LIBS) $(BOOST_UNIT_TEST_FRAMEWORK_LIB)
test_namecoin_LDADD += $(BDB_LIBS)

test_namecoin_SOURCES = test_namecoin.cpp uint160_tests.cpp uint256_tests.cpp

if ENABLE_WALLET
test_namecoin_SOURCES += 
endif

CLEANFILES = *.gcda *.gcno
