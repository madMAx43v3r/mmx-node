@ECHO OFF

CALL .\activate.cmd

mmx_wallet -c config\%NETWORK%\ config\wallet\ %MMX_HOME%\config\local\ %*