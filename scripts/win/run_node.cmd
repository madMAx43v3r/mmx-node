@ECHO OFF
TITLE MMX Node Run Script

CALL .\activate.cmd

mmx_node -c config\%NETWORK%\ config\node\ "%MMX_HOME%config\local\\" %*

IF "%~1"=="--PauseOnExit" PAUSE