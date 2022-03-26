@ECHO OFF

IF "%MMX_HOME%"=="" (
	SET MMX_HOME=%USERPROFILE%\.mmx\
)

SET NETWORK=testnet5
SET MMX_NETWORK=%NETWORK%\

ECHO NETWORK=%NETWORK%
ECHO MMX_HOME=%MMX_HOME%

IF NOT EXIST "%MMX_HOME%\config\local\" (
	XCopy "config\local_init" "%MMX_HOME%\config\local\" /S /F /Y
	ECHO "Initialized config\local\ with defaults."
) ELSE (
	REM
)

set MMX_ENV=%~dp0
set MMX_ENV="%MMX_ENV:~0,-1%"

SET PATH=%PATH%;%MMX_ENV%