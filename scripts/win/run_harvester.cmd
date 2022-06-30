@ECHO OFF
TITLE MMX Harvester Run Script

CALL .\activate.cmd

mmx_harvester -c config\%NETWORK%\ config\farmer\ %MMX_HOME%\config\local\ %*