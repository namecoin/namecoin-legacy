@rem System Path Settings
@set QTBASEPATH=C:/Qt/Qt5.3.0
@set QTPATH=%QTBASEPATH%/5.3/mingw482_32/bin
@set MINGWQTPATH=%QTBASEPATH%/Tools/mingw482_32/bin
@set MINGWBASEPATH=C:/MinGW
@set MINGWPATH=%MINGWBASEPATH%/bin
@set MSYSPATH=%MINGWBASEPATH%/msys/1.0/bin
@set PATH=%QTPATH%;%MINGWQTPATH%;%MINGWPATH%;%MSYSPATH%

@rem Downloadpaths
@set MINGWDOWNLOADPATH=https://sourceforge.net/projects/mingw/files/Installer/mingw-get-setup.exe/download
@set QTDOWNLOADPATH=http://download.qt-project.org/official_releases/qt/5.3/5.3.0/qt-opensource-windows-x86-mingw482_opengl-5.3.0.exe

@rem Library Paths
@set OPENSSL=openssl-1.0.1i
@set BERKELEYDB=db-4.8.30.NC
@set BOOST=boost_1_54_0
@set BOOSTVERSION=1.54.0
@rem If you wonder why there is no -s- see: https://github.com/bitcoin/bitcoin/pull/2835#issuecomment-21231694
@set BOOSTSUFFIX=-mgw48-mt-1_54
@set MINIUPNPC=miniupnpc-1.8

@rem Internal Paths
@set EWBLIBS=libs
@set EWBPATH=contrib/easywinbuilder
@set ROOTPATH=..\..
@set ROOTPATHSH=%ROOTPATH:\=/%
@set PERL=%MSYSPATH%/perl.exe

@rem Bootstrap Coin Name
@for /F %%a in ('dir /b %ROOTPATH%\*.pro') do @set COINNAME=%%a
@set COINNAME=%COINNAME:-qt.pro=%

@rem the following will be set as additional CXXFLAGS and CFLAGS for everything - no ' or ", space is ok
@set ADDITIONALCCFLAGS= -fno-guess-branch-probability -frandom-seed=1984 -Wno-unused-variable -Wno-unused-value -Wno-sign-compare -Wno-strict-aliasing

@rem Language Settings - always US
@set LANG=en_US.UTF8
@set LC_ALL=en_US.UTF8

@rem Note: Variables set here can NOT be overwritten in makefiles
