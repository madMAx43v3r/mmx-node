@ECHO OFF

CALL activate.cmd

mmx_timelord -c config\%NETWORK%\ config\timelord\ %MMX_HOME%\config\local\ %*