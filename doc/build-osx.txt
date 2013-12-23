MAC OSX BUILD NOTES
===================

Some notes on how to build Namecoin for Mac OSX.

To Build
--------

Namecoin now supports the GNU build system (aka autotools) on Mac OSX.
After the dependences are installed (see below), Namecoin can be built
as follows.

	$ ./autogen.sh
	$ cd namecoin-qt
	$ ./configure
	$ make

configure either builds ./src/namecoind or ./src/qt/namecoin-qt
depending on whether support for the Qt GUI toolkit was configured or
detected.  If you want both binaries, you should configure and build
twice, once with Qt and once without.

See the output from `./configure --help' for additional configure
options.

This release has been tested with the following dependency versions:
 - MAC OSX 10.8.5
 - gcc 4.2.1/XCode 5.0
 - Berkeley DB 4.8.30
 - Mac OSX default openssl and openssl-1.0.1e
 - boost 1.55.0
 - Qt4.8.5, through MacPorts and the stand-alone installer

Dependencies
------------

Namecoin depends on:
 - autoconf
 - automake
 - Berkeley DB 4.8
 - OpenSSL
 - boost
 - Qt GUI toolkit (optional)
 - qrencode (optional)
 - miniupnpc (optional)

The dependencies can be installed by hand.  To install them with
MacPorts, run:

	sudo port install automake autoconf db48 boost openssl qt4-mac qrencode

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
