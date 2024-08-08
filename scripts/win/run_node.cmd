@ECHO OFF
TITLE MMX Node Run Script

CALL .\activate.cmd

mmx_node -c "%MMX_HOME%config\%NETWORK%\" "%MMX_HOME%config\node\" "%MMX_HOME%config\local\" %*

IF "%~1"=="--PauseOnExit" PAUSE