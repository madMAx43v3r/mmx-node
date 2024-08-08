@ECHO OFF
TITLE MMX Farmer Run Script

CALL .\activate.cmd

mmx_farmer -c config\%NETWORK%\ config\farmer\ "%MMX_HOME%config\local\\" %*