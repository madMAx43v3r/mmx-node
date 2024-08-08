@ECHO OFF
TITLE MMX Farmer Run Script

CALL .\activate.cmd

mmx_farmer -c "%MMX_HOME%config\%NETWORK%\" "%MMX_HOME%config\farmer\" "%MMX_HOME%config\local\" %*