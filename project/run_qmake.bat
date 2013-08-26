set PATH=D:\MinGW\bin;D:\MinGW\msys\1.0\bin

: MinGW has its own Perl, so we disable the Windows one, in case it's present
set PERL=

D:\Qt4.8.4\bin\qmake.exe ^
	BOOST_INCLUDE_PATH=../libs/boost_1_50_0 ^
	BOOST_LIB_PATH=../libs/boost_1_50_0/stage/lib ^
	BOOST_LIB_SUFFIX=-mgw46-mt-1_50 ^
	OPENSSL_INCLUDE_PATH=../libs/openssl-1.0.1e/include ^
	BDB_INCLUDE_PATH=../libs/db-4.7.25.NC/build_unix ^
	BDB_LIB_PATH=../libs/db-4.7.25.NC/build_unix ^
	OPENSSL_LIB_PATH=../libs/openssl-1.0.1e ^
	MINIUPNPC_INCLUDE_PATH=../libs/miniupnpc-1.8.20130211 ^
	MINIUPNPC_LIB_PATH=../libs/miniupnpc-1.8.20130211
