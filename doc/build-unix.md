UNIX BUILD NOTES
====================
Some notes on how to build Bitcoin in Unix. 

Dependencies
---------------------

 Library     Purpose           Description
 -------     -------           -----------
 libssl      SSL Support       Secure communications
 libdb4.8    Berkeley DB       Wallet storage
 libboost    Boost             C++ Library
 miniupnpc   UPnP Support      Optional firewall-jumping support
 qt          GUI               GUI toolkit
 libqrencode QR codes in GUI   Optional for generating QR codes

Dependency Build Instructions: Ubuntu & Debian
----------------------------------------------
Build requirements:

	sudo apt-get install build-essential
	sudo apt-get install libtool autotools-dev autoconf
	sudo apt-get install libssl-dev

for Ubuntu 12.04 and later:

	sudo apt-get install libboost-all-dev

 db4.8 packages are available [here](https://launchpad.net/~bitcoin/+archive/bitcoin).

Ubuntu 12.04 and later have packages for libdb5.1-dev and
libdb5.1++-dev, but using these will break binary wallet
compatibility, and is not recommended.

for other Ubuntu & Debian:

	sudo apt-get install libdb4.8-dev
	sudo apt-get install libdb4.8++-dev
	sudo apt-get install libboost1.37-dev
 (If using Boost 1.37, append -mt to the boost libraries in the makefile)

Optional:

	sudo apt-get install libminiupnpc-dev (see --with-miniupnpc and --enable-upnp-default)

Dependencies for the GUI: Ubuntu & Debian
-----------------------------------------

If you want to build Bitcoin-Qt, make sure that the required packages for Qt development
are installed. Qt 4 is currently necessary to build the GUI.

To build with Qt 4 you need the following:

    apt-get install libqt4-dev

libqrencode (optional) can be installed with:

    apt-get install libqrencode-dev

Once these are installed, they will be found by configure and a bitcoin-qt executable will be
built by default.

To Build
---------------------

	./autogen.sh
	./configure
	make

This will build ./src/namecoind or ./src/qt/namecoin-qt depending on
whether support for the Qt GUI toolkit was configured or detected.  If
you want both binaries, you should configure and build twice, once
with Qt and once without.

[miniupnpc](http://miniupnp.free.fr/) may be used for UPnP port
mapping.  It can be downloaded from [here](
http://miniupnp.tuxfamily.org/files/).  See the configure options for
upnp behavior desired:

	--with-miniupnpc         No UPnP support miniupnp not required
	--disable-upnp-default   (the default) UPnP support turned off by default at runtime
	--enable-upnp-default    UPnP support turned on by default at runtime

IPv6 support may be disabled by setting:

	--disable-ipv6           Disable IPv6 support

This release has been tested with the following versions:
-  Ubuntu	 14.04.3 TLS
-  GCC           4.6.3
-  OpenSSL       1.0.0, 1.0.1e
-  Berkeley DB   4.8.30
-  Boost         1.48.0
-  qt            4.8.1
-  libqrencode   3.1.1

Security
--------
To help make your bitcoin installation more secure by making certain attacks impossible to
exploit even if a vulnerability is found, binaries are hardened by default.
This can be disabled with:

Hardening Flags:

	./configure --enable-hardening
	./configure --disable-hardening


Hardening enables the following features:

* Position Independent Executable
    Build position independent code to take advantage of Address Space Layout Randomization
    offered by some kernels. An attacker who is able to cause execution of code at an arbitrary
    memory location is thwarted if he doesn't know where anything useful is located.
    The stack and heap are randomly located by default but this allows the code section to be
    randomly located as well.

    On an Amd64 processor where a library was not compiled with -fPIC, this will cause an error
    such as: "relocation R_X86_64_32 against `......' can not be used when making a shared object;"

    To test that you have built PIE executable, install scanelf, part of paxutils, and use:

    	scanelf -e ./bitcoin

    The output should contain:
     TYPE
    ET_DYN

* Non-executable Stack
    If the stack is executable then trivial stack based buffer overflow exploits are possible if
    vulnerable buffers are found. By default, bitcoin should be built with a non-executable stack
    but if one of the libraries it uses asks for an executable stack or someone makes a mistake
    and uses a compiler extension which requires an executable stack, it will silently build an
    executable without the non-executable stack protection.

    To verify that the stack is non-executable after compiling use:
    `scanelf -e ./bitcoin`

    the output should contain:
	STK/REL/PTL
	RW- R-- RW-

    The STK RW- means that the stack is readable and writeable but not executable.
