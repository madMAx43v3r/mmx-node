@ECHO OFF
TITLE MMX Make Release and Pack Script

REM Usage
REM CMD
REM SET MMX_VERSION=v0.9.10 && SET MMX_GIGAHORSE=FALSE && .\make_release_pack.cmd

REM PowerShell
REM $env:MMX_VERSION='v0.9.10'; $env:MMX_GIGAHORSE='FALSE'; .\make_release_pack.cmd

REM SET MMX_VERSION=v0.0.0
REM SET MMX_GIGAHORSE=FALSE

SET MMX_WIN_PACK=TRUE
SET CMAKE_BUILD_PRESET=windows-release

CALL .\make_release.cmd && ^
IF "%MMX_WIN_PACK%"=="TRUE" cpack -C Release --config ./build/%CMAKE_BUILD_PRESET%/CPackConfig.cmake -B ./build/%CMAKE_BUILD_PRESET%/!package
