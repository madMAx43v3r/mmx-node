@ECHO OFF
TITLE MMX Make Release and Pack Script

SET MMX_VERSION=v0.0.0
SET MMX_WIN_PACK=TRUE
SET CMAKE_BUILD_PRESET=windows-release

CALL .\make_release.cmd && ^
IF "%MMX_WIN_PACK%"=="TRUE" cpack -C Release --config ./build/%CMAKE_BUILD_PRESET%/CPackConfig.cmake -B ./build/%CMAKE_BUILD_PRESET%/!package
