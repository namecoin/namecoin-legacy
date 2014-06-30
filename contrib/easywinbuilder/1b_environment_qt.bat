@call set_vars.bat
@echo About to download Qt installer - you need to install it manually:
@echo  Use default directory "%QTBASEPATH%".
@echo  Activate checkbox: tools - MinGW !!!
@echo.
@pause
@start %QTDOWNLOADPATH%
@set WAITQT=1
