@ECHO OFF

SET "MMX_ENV=%~dp0"
SET "MMX_ENV=%MMX_ENV:~0,-1%"
SET "PATH=%PATH%; %MMX_ENV%"

IF "%MMX_HOME%"=="" (
	SET "MMX_HOME=%USERPROFILE%\.mmx"
)

SET "MMX_HOME=%MMX_HOME%\"

SET "NETWORK=testnet8"
SET "MMX_NETWORK=%NETWORK%\"

ECHO NETWORK=%NETWORK%
ECHO MMX_HOME=%MMX_HOME%

IF NOT EXIST "%MMX_HOME%config\local" (
	XCopy "config\local_init" "%MMX_HOME%config\local\" /S /F /Y
	ECHO "Initialized config\local\ with defaults."
)

IF NOT EXIST "%MMX_HOME%PASSWD" (
	generate_passwd > "%MMX_HOME%PASSWD"

	SET /P PASSWD=<"%MMX_HOME%PASSWD"
	CALL ECHO PASSWD=%%PASSWD%%

	CALL vnxpasswd -c config\default\ "%MMX_HOME%config\local\\" -u mmx-admin -p %%PASSWD%%
)
