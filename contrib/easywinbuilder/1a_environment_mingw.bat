@call set_vars.bat
@echo About to download MinGW/MSYS installer - you need to install it manually. Note:
@echo  Install to default directory: %MINGWBASEPATH%
@echo  Unselect "... also install support for the graphical user interface."
@echo.
@pause
@start %MINGWDOWNLOADPATH%
@echo.
@echo Once the mingw-get-setup has finished press a key.
@pause
%MINGWPATH%\mingw-get.exe update
%MINGWPATH%\mingw-get.exe install msys-base
@rem %MINGWPATH%\mingw-get install mingw32-make
%MINGWPATH%\mingw-get install msys-wget-bin
%MINGWPATH%\mingw-get install msys-unzip-bin
%MINGWPATH%\mingw-get install msys-perl

@rem %MINGWPATH%\mingw-get.exe install --reinstall --recursive "gcc=4.6.*"
@rem %MINGWPATH%\mingw-get.exe install --reinstall --recursive "gcc-g++=4.6.*"
@rem %MINGWPATH%\mingw-get.exe install --reinstall --recursive "gcc-bin=4.6.*"

@rem %MINGWPATH%\mingw-get install --reinstall --recursive libgmp-dll="5.0.1-*"
@rem %MINGWPATH%\mingw-get install --reinstall --recursive  mingwrt-dev
@rem %MINGWPATH%\mingw-get.exe install --reinstall --recursive "w32api=3.17-2"
@rem %MINGWPATH%\mingw-get.exe install --reinstall --recursive "w32api-dev=3.17-2"

@rem There is a problem with MSYS bash and sh that stalls OpenSSL config on some systems. Using rxvt shell as a workaround.
%MINGWPATH%\mingw-get install msys-rxvt
echo.
