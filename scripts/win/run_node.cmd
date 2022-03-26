@ECHO OFF

CALL .\activate.cmd

mmx_node -c config\%NETWORK%\ config\node\ %MMX_HOME%\config\local\ %*