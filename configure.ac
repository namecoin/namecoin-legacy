AC_INIT(Namecoin, 0.3.72, js@webkeks.org)
AC_CANONICAL_HOST
BUILDSYS_INIT

AC_CONFIG_SRCDIR(src)

AC_LANG([C++])
AC_PROG_CXX
AC_PROG_CXXCPP
AC_PROG_INSTALL

CPP="$CXXCPP"
CPPFLAGS="$CPPFLAGS $CXXCPPFLAGS"
OBJCXX="$CXX"
OBJCXXFLAGS="$OBJCXXFLAGS $CXXFLAGS"

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

case "$host_os" in
	mingw*)
		;;
	*)
		ACX_PTHREAD([
			CPPFLAGS="$CPPFLAGS $PTHREAD_CFLAGS"
			LIBS="$LIBS $PTHREAD_LIBS"
		])
		;;
esac

AC_ARG_WITH(qt,
	AS_HELP_STRING([--without-qt], [build without Qt]))
AS_IF([test x"$with_qt" != x"no"], [
	PKG_CHECK_MODULES(Qt, [QtCore >= 4.8 QtGui >= 4.8 QtNetwork >= 4.8], [
		CPPFLAGS="$CPPFLAGS $Qt_CFLAGS"
		LIBS="$LIBS $Qt_LIBS"
	], [
		AC_MSG_ERROR([Qt{Core,Gui,Network} >= 4.8 not found!])
	])

	AC_CHECK_TOOLS(MOC, [moc-qt4 moc])
	AS_IF([test -z "$MOC"], [AC_MSG_ERROR([moc not found!])])

	AC_CHECK_TOOLS(UIC, [uic-qt4 uic])
	AS_IF([test -z "$UIC"], [AC_MSG_ERROR([uic not found!])])

	AC_CHECK_TOOLS(RCC, [rcc-qt4 rcc])
	AS_IF([test -z "$RCC"], [AC_MSG_ERROR([rcc not found!])])

	AC_CHECK_TOOLS(LRELEASE, [lrelease-qt4 lrelease])
	AS_IF([test -z "$LRELEASE"], [AC_MSG_ERROR([lrelease not found!])])

	AC_DEFINE(GUI, 1, [Whether we build the GUI version])

	AC_SUBST(NAMECOIN, namecoin-qt)
	AC_SUBST(USE_FORMS_HEADERS, '${FORMS_HEADERS}')
	AC_SUBST(USE_MOC_FILES, '${MOC_FILES}')
	AC_SUBST(USE_QM_FILES, '${QM_FILES}')
	AC_SUBST(USE_SRCS_QT, '${SRCS_QT}')

	case "$host_os" in
		darwin*)
			AC_SUBST(MOC_CPPFLAGS, [-DQ_OS_MAC])
			AC_SUBST(QT_MACDOCKICONHANDLER_MM,
				qt/macdockiconhandler.mm)
			AC_SUBST(QT_MACDOCKICONHANDLER_MOC_CC,
				qt/macdockiconhandler.moc.cc)
			LIBS="$LIBS -framework Cocoa"
			;;
		mingw*)
			AC_SUBST(MOC_CPPFLAGS, [-D_WIN32])
			;;
	esac
], [
	AC_SUBST(NAMECOIN, namecoind)
])

dnl We don't call AC_PROG_CPP, but only AC_PROG_CXXCPP and set CPP to CXXCPP
dnl and add CXXCPPFLAGS to CPPFLAGS, thus we need to AC_SUBST these ourself.
AC_SUBST(CPP)
AC_SUBST(CPPFLAGS)
dnl OBJCXX is required for Mac things - it's set to CXX, though
AC_SUBST(OBJCXX)
AC_SUBST(OBJCXXFLAGS)
dnl OBJCXX also uses OBJCFLAGS
AC_SUBST(OBJCFLAGS)

AC_CONFIG_FILES([buildsys.mk extra.mk])
AC_CONFIG_HEADERS(config.h)
AC_OUTPUT
