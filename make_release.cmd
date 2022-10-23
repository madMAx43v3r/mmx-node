@ECHO OFF
TITLE MMX Make Release Script

IF "%VCPKG_ROOT%"=="" (
	SET VCPKG_ROOT=C:/dev/vcpkg
)

IF "%CMAKE_BUILD_PRESET%"=="" (
	SET CMAKE_BUILD_PRESET=windows-release
)

cmake --preset %CMAKE_BUILD_PRESET% && ^
cmake --build --preset %CMAKE_BUILD_PRESET%
