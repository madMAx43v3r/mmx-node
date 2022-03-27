@ECHO OFF

CALL .\activate.cmd

mmx_farmer -c config\%NETWORK%\ config\farmer\ %MMX_HOME%\config\local\ %*