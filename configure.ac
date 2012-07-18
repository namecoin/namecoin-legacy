## AC-AM Package Preliminaries
AC_PREREQ([2.61])
AC_INIT([namecoin],m4_esyscmd([tr -d '\n' <VERSION]),[],[namecoin],[https://github.com/namcoin])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([build-aux/m4])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.10 -Wall -Werror foreign])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP

: ${CXXFLAGS=""}
: ${CFLAGS=""}
: ${CPPFLAGS=""}

## libtool prelims.
LT_PREREQ([2.2.4])
LT_INIT([dlopen])

# Checks for packages.

PKG_PROG_PKG_CONFIG([0.25])

##### Dependencies, packages, libs ####
#
## Main packages deps ##

#PKG_CHECK_MODULES([PKGS],[boost >= 1.25.0 db_cxx])

## boost_thread lib ##

AX_BOOST_BASE([1.25.0])
#AX_BOOST_SYSTEM
AX_BOOST_THREAD
#AX_BOOST_FILESYSTEM
#AX_BOOST_PROGRAM_OPTIONS

## OpenSSL
# "--with-openssl=DIR" point to optional openssl package headers (for devs)

PKG_CHECK_MODULES([OPENSSL],[openssl >= 0.9.7],
	[AX_CHECK_OPENSSL],
	[AC_MSG_RESULT([Warning: openssl version with ECC not found!])]
	)

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

###########################################
## Configure Options
##
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
#AC_SUBST(	[DEPS_CFLAGS],
#["$OPENSSL_CFLAGS $BOOST_CPPFLAGS"])

#AC_SUBST(	[DEPS_LIBS],
#["$OPENSSL_LDFLAGS $OPENSSL_LIBS $BOOST_LDFLAGS $BOOST_THREAD_LIB"])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
