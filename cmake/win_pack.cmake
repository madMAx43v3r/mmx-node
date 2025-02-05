set(NETWORK "mainnet")
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/nsis/;${CMAKE_MODULE_PATH}")

function(get_all_targets var)
    set(targets)
    get_all_targets_recursive(targets ${CMAKE_CURRENT_SOURCE_DIR})
    set(${var} ${targets} PARENT_SCOPE)
endfunction()

macro(get_all_targets_recursive targets dir)
    get_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(subdir ${subdirectories})
        get_all_targets_recursive(${targets} ${subdir})
    endforeach()

    get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    list(APPEND ${targets} ${current_targets})
endmacro()

get_all_targets(all_targets)
set_target_properties(${all_targets} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$(OutDir)")
set_target_properties(${all_targets} PROPERTIES VS_DEBUGGER_ENVIRONMENT "MMX_HOME=$(USERPROFILE)\\.mmx\\\nMMX_DATA=$(USERPROFILE)\\.mmx\\\nNETWORK=${NETWORK}\nMMX_NETWORK=$(USERPROFILE)\\.mmx\\${NETWORK}\\\n")
set_target_properties(mmx_node PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "-c config\\${NETWORK}\\ config\\node\\ $(USERPROFILE)\\.mmx\\config\\local\\")
set_target_properties(mmx_timelord PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "-c config\\${NETWORK}\\ config\\timelord\\ $(USERPROFILE)\\.mmx\\config\\local\\")
set_target_properties(mmx_wallet PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "-c config\\${NETWORK}\\ config\\wallet\\ $(USERPROFILE)\\.mmx\\config\\local\\")
set_target_properties(mmx_farmer mmx_harvester PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "-c config\\${NETWORK}\\ config\\farmer\\ $(USERPROFILE)\\.mmx\\config\\local\\")


add_custom_command(TARGET mmx PRE_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_directory
	${CMAKE_SOURCE_DIR}/kernel/ $<TARGET_FILE_DIR:mmx>/kernel/)
add_custom_command(TARGET mmx PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_SOURCE_DIR}/config/ $<TARGET_FILE_DIR:mmx>/config/)
add_custom_command(TARGET mmx PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_SOURCE_DIR}/www/ $<TARGET_FILE_DIR:mmx>/www/)
add_custom_command(TARGET mmx PRE_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_directory
		${CMAKE_SOURCE_DIR}/data/ $<TARGET_FILE_DIR:mmx>/data/)
add_custom_command(TARGET mmx PRE_BUILD
 	COMMAND ${CMAKE_COMMAND} -E copy_directory
 		${CMAKE_SOURCE_DIR}/scripts/win/ $<TARGET_FILE_DIR:mmx>)

if(NOT MMX_VERSION MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
	string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d")
	set(MMX_VERSION "v0.0.0.${BUILD_TIMESTAMP}")
endif()

message(STATUS "MMX_VERSION=${MMX_VERSION}")

#file(STRINGS "include/mmx/version.h" MMX_VERSION_H REGEX "^#define MMX_VERSION \"[^\"]*\"$")
string(REGEX REPLACE "^v([0-9]+).*$" "\\1" MMX_VERSION_MAJOR "${MMX_VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.([0-9]+).*$" "\\1" MMX_VERSION_MINOR  "${MMX_VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.([0-9]+.*)$" "\\1" MMX_VERSION_PATCH "${MMX_VERSION}")

set(MMX_VERSION_STRING "${MMX_VERSION_MAJOR}.${MMX_VERSION_MINOR}.${MMX_VERSION_PATCH}")
#message(STATUS "MMX_VERSION_STRING: ${MMX_VERSION_STRING}")

set(CPACK_PACKAGE_VERSION ${MMX_VERSION_STRING})
set(CPACK_PACKAGE_VERSION_MAJOR ${MMX_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${MMX_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${MMX_VERSION_PATCH})

include(cmake/product_version/generate_product_version.cmake)
set(MMX_ICON "${CMAKE_CURRENT_SOURCE_DIR}/cmake/mmx.ico")

set(MMX_BUNDLE "MMX Node")

list(APPEND APP_FILES
	mmx mmx_node mmx_farmer mmx_wallet mmx_timelord mmx_harvester
	mmx_compile mmx_postool
	mmx_iface mmx_modules
	vnx_base vnx_addons url_cpp llhttp
	vnxpasswd generate_passwd
	automy_basic_opencl
)

list(APPEND TOOL_FILES
	tx_bench
)
foreach(APPFILE IN LISTS APP_FILES TOOL_FILES)
	set(ProductVersionFiles "ProductVersionFiles_${APPFILE}")
	generate_product_version(
		${ProductVersionFiles}
		NAME ${APPFILE}
		BUNDLE ${MMX_BUNDLE}
		COMPANY_NAME "madMAx43v3r"
		FILE_DESCRIPTION ${APPFILE}
		ICON ${MMX_ICON}
		VERSION_MAJOR ${CPACK_PACKAGE_VERSION_MAJOR}
		VERSION_MINOR ${CPACK_PACKAGE_VERSION_MINOR}
		VERSION_PATCH ${CPACK_PACKAGE_VERSION_PATCH}
	)
	target_sources(${APPFILE} PRIVATE ${${ProductVersionFiles}})
endforeach()

if ("${MMX_WIN_PACK}" STREQUAL "TRUE")

set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT libraries)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ./)
set(CMAKE_INSTALL_OPENMP_LIBRARIES TRUE)
include(InstallRequiredSystemLibraries)

add_custom_target(mmx_node_gui ALL)
set(MMX_NODE_GUI_GIT_TAG "origin/main")
message(STATUS "MMX Node GUI will be built from: ${MMX_NODE_GUI_GIT_TAG}")
include(FetchContent)
FetchContent_Declare(
	mmx_node_gui
	GIT_REPOSITORY https://github.com/stotiks/mmx-node-gui.git
	GIT_TAG ${MMX_NODE_GUI_GIT_TAG}
)
FetchContent_MakeAvailable(mmx_node_gui)
add_custom_command(TARGET mmx_node_gui POST_BUILD
	COMMAND ${CMAKE_MAKE_PROGRAM} Mmx.Gui.Win.Wpf/Mmx.Gui.Win.Wpf.csproj -restore -m 
			/p:Configuration=Release
			/p:PlatformTarget=x64
			/p:OutputPath=${mmx_node_gui_SOURCE_DIR}/bin/Release
			/p:Version=${MMX_VERSION_STRING}
			/p:FileVersion=${MMX_VERSION_STRING}
		WORKING_DIRECTORY ${mmx_node_gui_SOURCE_DIR}
)

set(mmx_node_gui_RELEASE_DIR ${mmx_node_gui_SOURCE_DIR}/bin/CPack_Release)

install(DIRECTORY ${mmx_node_gui_RELEASE_DIR}/ DESTINATION ./ COMPONENT gui)

install(TARGETS ${APP_FILES} RUNTIME DESTINATION ./ COMPONENT applications)
install(TARGETS ${TOOL_FILES} RUNTIME DESTINATION ./ COMPONENT tools)

install(FILES 
			$<TARGET_FILE_DIR:vnx_addons>/zlib1.dll
			$<TARGET_FILE_DIR:vnx_addons>/zstd.dll
			$<TARGET_FILE_DIR:automy_basic_opencl>/OpenCL.dll
			$<TARGET_FILE_DIR:mmx_modules>/miniupnpc.dll
			$<TARGET_FILE_DIR:mmx_modules>/cudart64_12.dll
		DESTINATION ./ COMPONENT applications)

install(DIRECTORY kernel/ DESTINATION kernel COMPONENT applications)
install(DIRECTORY config/ DESTINATION config COMPONENT applications)
install(DIRECTORY www/ DESTINATION www COMPONENT applications)
install(DIRECTORY data/ DESTINATION data COMPONENT applications)
install(DIRECTORY scripts/win/ DESTINATION ./ COMPONENT applications)
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION ./ COMPONENT applications)


FetchContent_Declare(
    mmx_cuda_plotter
    GIT_REPOSITORY https://github.com/madMAx43v3r/mmx-binaries.git
    GIT_TAG "origin/master"
)
FetchContent_MakeAvailable(mmx_cuda_plotter)

set (MMX_DESTINATION ./)
set(MMX_CUDA_PLOTTER_PATH ${mmx_cuda_plotter_SOURCE_DIR}/mmx-cuda-plotter/windows)
install(DIRECTORY ${MMX_CUDA_PLOTTER_PATH}/ DESTINATION ${MMX_DESTINATION} COMPONENT plotters)


set(CPACK_PACKAGE_NAME "MMX Node")
set(CPACK_PACKAGE_VENDOR "madMAx43v3r")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MMX is a blockchain written from scratch using Chia's Proof Of Space and a SHA256 VDF similar to Solana")
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE)
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/madMAx43v3r/mmx-node")

set(CPACK_PACKAGE_INSTALL_DIRECTORY "MMX Node")

# Define components and their display names
set(CPACK_COMPONENTS_ALL applications libraries gui plotters tools)

set(CPACK_COMPONENT_APPLICATIONS_DISPLAY_NAME "Node")
set(CPACK_COMPONENT_LIBRARIES_DISPLAY_NAME "Libraries")
set(CPACK_COMPONENT_GUI_DISPLAY_NAME "GUI")
set(CPACK_COMPONENT_PLOTTERS_DISPLAY_NAME "Plotter")
set(CPACK_COMPONENT_TOOLS_DISPLAY_NAME "Tools")


set(CPACK_COMPONENT_APPLICATIONS_REQUIRED True)
set(CPACK_COMPONENT_LIBRARIES_REQUIRED True)
set(CPACK_COMPONENT_GUI_REQUIRED True)
set(CPACK_COMPONENT_PLOTTERS_REQUIRED True)
#set(CPACK_COMPONENT_TOOLS_REQUIRED True)

# Define NSIS installation types
set(CPACK_ALL_INSTALL_TYPES Full Advanced)
# set(CPACK_COMPONENT_APPLICATIONS_INSTALL_TYPES Full Minimal Advanced)
# set(CPACK_COMPONENT_SYSLIBRARIES_INSTALL_TYPES Full Minimal Advanced)
# set(CPACK_COMPONENT_GUI_INSTALL_TYPES Full Minimal Advanced)
# set(CPACK_COMPONENT_PLOTTERS_INSTALL_TYPES Full Minimal Advanced)
set(CPACK_COMPONENT_TOOLS_INSTALL_TYPES Advanced)


set(CPACK_NSIS_PACKAGE_NAME ${CPACK_PACKAGE_NAME})

set(CPACK_NSIS_MENU_LINKS
"mmx_cmd.cmd" "MMX CMD"
#"http://localhost:11380/gui/" "MMX Node WebGUI"
"MmxGui.exe" "MMX Node"
"MmxPlotter.exe" "MMX Plotter"
)
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

set(MMX_GUI_EXE "MmxGui.exe")

set(CPACK_NSIS_MUI_ICON ${MMX_ICON})
set(CPACK_NSIS_INSTALLED_ICON_NAME ${MMX_GUI_EXE})

#set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
#set(CPACK_NSIS_MUI_FINISHPAGE_RUN ${MMX_GUI_EXE})

# Must be after the last CPACK macros
include(CPack)

endif() #if (${MMX_WIN_PACK} STREQUAL "TRUE")