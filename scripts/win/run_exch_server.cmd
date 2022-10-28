@ECHO OFF
TITLE MMX Exchange Server Run Script

CALL .\activate.cmd

mmx_exch_server -c config\%NETWORK%\ "%MMX_HOME%config\local\\" %*