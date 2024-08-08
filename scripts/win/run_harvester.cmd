@ECHO OFF
TITLE MMX Harvester Run Script

CALL .\activate.cmd

mmx_harvester -c "%MMX_HOME%config\%NETWORK%\" "%MMX_HOME%config\farmer\" "%MMX_HOME%config\local\" %*