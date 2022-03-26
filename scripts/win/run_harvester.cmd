@ECHO OFF

CALL .\activate.cmd

mmx_harvester -c config\%NETWORK%\ config\farmer\ %MMX_HOME%\config\local\ %*