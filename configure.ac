AC_INIT(Namecoin, 0.3.72, js@webkeks.org)
BUILDSYS_INIT

AC_CONFIG_SRCDIR(src)

AC_LANG([C++])
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_INSTALL

AC_CHECK_HEADER([boost/foreach.hpp], [], [
	AC_MSG_ERROR([Boost headers not found!])
])

CHECK_BOOST_LIB(system)
CHECK_BOOST_LIB(filesystem)
CHECK_BOOST_LIB(program_options)
CHECK_BOOST_LIB(thread)
CHECK_BOOST_LIB(chrono)

AC_CHECK_LIB(db_cxx, main, [
	LIBS="$LIBS -ldb_cxx"
], [
	AC_MSG_ERROR([libdb_cxx not found!])
])

AC_CHECK_LIB(crypto, main, [
	LIBS="$LIBS -lcrypto"
], [
	AC_MSG_ERROR([libcrypto (from OpenSSL) not found!])
])

AC_CHECK_LIB(ssl, main, [
	LIBS="$LIBS -lssl"
	AC_DEFINE(USE_SSL, 1, [Whether SSL should be used])
], [
	AC_MSG_ERROR([libssl (from OpenSSL) not found!])
])

AC_ARG_WITH(miniupnpc,
	AS_HELP_STRING([--without-miniupnpc], [build without miniupnpc]))
AS_IF([test x"$with_miniupnpc" != x"no"], [
	AC_CHECK_HEADER([miniupnpc/miniupnpc.h], [
		AC_CHECK_LIB(miniupnpc, main, [
			LIBS="$LIBS -lminiupnpc"
			AC_DEFINE(USE_UPNP, 1, [Whether UPnP should be used])
		], [
			AC_MSG_ERROR([miniupnpc libraries not found!])
		])
	], [
		AC_MSG_ERROR([miniupnpc headers not found!])
	])
])

AC_CONFIG_FILES(buildsys.mk)
AC_CONFIG_HEADERS(config.h)
AC_OUTPUT
