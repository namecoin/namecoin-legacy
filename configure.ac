############## Mandatory Copyright Troll ######################
## AC-AM Package Preliminaries
AC_PREREQ([2.61])
AC_INIT([namecoin],m4_esyscmd([tr -d '\n' <VERSION]),[],[namecoin],[https://github.com/namcoin])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux/m4])

AM_INIT_AUTOMAKE([1.10 -Wall -Werror foreign])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP

: ${CXXFLAGS=""}
: ${CFLAGS=""}
: ${CPPFLAGS=""}

## libtool prelims.
LT_PREREQ([2.2.4])
LT_INIT([dlopen])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h float.h limits.h malloc.h netdb.h netinet/in.h stddef.h stdlib.h string.h sys/param.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UINT64_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([gethostbyname gethostname gettimeofday iswprint memmove memset pow select setlocale socket sqrt strchr strerror strrchr strspn strstr strtoul])

# Checks for packages.

PKG_PROG_PKG_CONFIG([0.25])

##### Dependencies, packages, libs ####
#

## boost_thread lib ##

AX_BOOST_BASE([1.25.0])
AX_BOOST_SYSTEM
AX_BOOST_THREAD
AX_BOOST_FILESYSTEM
AX_BOOST_PROGRAM_OPTIONS

## Berkeley DB_CXX lib ##

AC_LANG_PUSH([C++])
AX_BERKELEY_DB_CXX
AC_LANG_POP

## OpenSSL
# "--with-openssl=DIR" point to optional openssl package headers (for devs)

PKG_CHECK_MODULES([OPENSSL],[openssl],
	[AX_CHECK_OPENSSL],
	[AC_MSG_RESULT([Possible Warning: openssl version with ECC not found!])]
	)

# gthread-2.0

PKG_CHECK_MODULES([GTHREAD],[gthread-2.0])

## pthread, z libs ##

AX_PTHREAD

AX_CHECK_ZLIB

###########################################
## Configure Options
##

## (Option) Add miniUPNPC support ##
AC_ARG_WITH(
	[UPNP],
	[AS_HELP_STRING([--with-upnp], [])],
	[])

AM_CONDITIONAL([WANT_UPNP], [test "x$with_upnp" == "xyes"])

if (test "x$with_upnp" == "xyes"); then

	UPNP_LIBS = "-lminiupnpc"
fi
## (^^^need to do some more lib checking here!)

## (Option) Signal Handler ##


AC_ARG_ENABLE(
	sighandler,
	AS_HELP_STRING(
		[--enable-sighandler],
		[enable sighandler, default: no]),
	[case "${enableval}" in
		yes)	sighandler=true ;;
		no)	sighandler=false ;;
		*)	AC_MSG_ERROR([bad value ${enableval} for --disable-sighandler]) ;;
		esac],
	[sighandler=false])

AM_CONDITIONAL(OT_SIGNAL_HANDLING, test x"$sighandler" = x"true")

## (Option) Warnings ##

AC_ARG_ENABLE(
	warnings,
	AS_HELP_STRING(
		[--enable-warnings],
		[enable warnings, default: no]),
	[case "${enableval}" in
		yes) warnings=true ;;
		no)  warnings=false ;;
		*)   AC_MSG_ERROR([bad value ${enableval} for --enable-warnings]) ;;
		esac],
	[warnings=false])

AM_CONDITIONAL(WARNINGS, test x"$warnings" = x"true")

## (Option) Build In Release Mode ##

AC_ARG_ENABLE(
	release,
	AS_HELP_STRING(
		[--enable-release],
		[enable releasemode, default: no]),
	[case "${enableval}" in
		yes) release=true ;;
		no)  release=false ;;
		*)   AC_MSG_ERROR([bad value ${enableval} for --enable-release]) ;;
		esac],
	[release=false])

AM_CONDITIONAL(RELEASE, test x"$release" = x"true")


##### Options END #####
##
## Output
##
AC_SUBST(	[DEPS_CFLAGS],
["$GTHREAD_CFLAGS $BOOST_CPPFLAGS $OPENSSL_CFLAGS"])

AC_SUBST(	[DEPS_LIBS],
["$GTHREAD_LIBS $UPNP_LIBS $BOOST_LDFLAGS  $DB_CXX_LIBS $BOOST_SYSTEM_LIB $BOOST_THREAD_LIB $BOOST_FILESYSTEM_LIB $BOOST_PROGRAM_OPTIONS_LIB $OPENSSL_LDFLAGS $OPENSSL_LIBS $PTHREAD_LIBS"])

AC_SUBST(	[DEPS_LDFLAGS],
["$PTHREAD_CFLAGS"])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
