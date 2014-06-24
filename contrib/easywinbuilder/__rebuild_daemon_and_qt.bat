@set RUNALL=1
@call 4a_build_daemon.bat
@if errorlevel 1 goto error
@call 4b_build_qt.bat
@if errorlevel 1 goto error
@echo.
@echo.
@goto end

:error
@echo Fatal error! Errorlevel: %errorlevel%

:end
@pause


