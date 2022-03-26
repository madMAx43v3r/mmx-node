@ECHO OFF

CALL .\activate.cmd

mmx_node -c config\%NETWORK%\ config\node\ config\light_mode\ %MMX_HOME%\config\local\ %*