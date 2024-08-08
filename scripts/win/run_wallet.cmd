@ECHO OFF
TITLE MMX Wallet Run Script

CALL .\activate.cmd

mmx_wallet -c "%MMX_HOME%config\%NETWORK%\" "%MMX_HOME%config\wallet\" "%MMX_HOME%config\local\" %*