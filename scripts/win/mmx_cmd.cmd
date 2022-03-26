@ECHO OFF
CALL .\activate.cmd

setlocal
set PROMPT=[MMX] $P$G
start "MMX Environment" cmd.exe /K "mmx_help.cmd"

:EXIT
endlocal