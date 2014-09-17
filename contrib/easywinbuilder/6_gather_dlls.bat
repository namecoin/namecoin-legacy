@call set_vars.bat

@set QTPATHF=%QTPATH:/=\%
@set MINGWQTPATHF=%MINGWQTPATH:/=\%

@echo namecoind DLLs
copy %MINGWQTPATHF%\libgcc_s_dw2-1.dll %ROOTPATH%\src\
copy "%MINGWQTPATHF%\libstdc++-6.dll" %ROOTPATH%\src\

copy %QTPATHF%\libwinpthread-1.dll %ROOTPATH%\src\

@echo namecoin-qt DLLs
copy %MINGWQTPATHF%\libgcc_s_dw2-1.dll %ROOTPATH%\release\
copy "%MINGWQTPATHF%\libstdc++-6.dll" %ROOTPATH%\release\

copy %QTPATHF%\Qt5Core.dll %ROOTPATH%\release\
copy %QTPATHF%\Qt5Gui.dll %ROOTPATH%\release\
copy %QTPATHF%\Qt5Widgets.dll %ROOTPATH%\release\
copy %QTPATHF%\Qt5Network.dll %ROOTPATH%\release\
copy %QTPATHF%\libwinpthread-1.dll %ROOTPATH%\release\
copy %QTPATHF%\icuin52.dll %ROOTPATH%\release\
copy %QTPATHF%\icuuc52.dll %ROOTPATH%\release\
copy %QTPATHF%\icudt52.dll %ROOTPATH%\release\

mkdir %ROOTPATH%\release\platforms
copy %QTPATHF%\..\plugins\platforms\qwindows.dll %ROOTPATH%\release\platforms

@if not "%RUNALL%"=="1" pause
