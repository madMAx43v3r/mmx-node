@ECHO OFF
TITLE MMX Timelord Run Script

CALL activate.cmd

mmx_timelord -c "%MMX_HOME%config\%NETWORK%\" "%MMX_HOME%config\timelord\" "%MMX_HOME%config\local\" %*