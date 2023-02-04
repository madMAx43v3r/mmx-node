

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
	set(MMX_VERSION "v0.9.0.${BUILD_TIMESTAMP}")
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
set(MMX_FRIENDLY_STRING "MMX Node ${MMX_VERSION}")

list(APPEND APP_FILES
	mmx mmx_node mmx_farmer mmx_wallet mmx_timelord mmx_harvester
	mmx_db mmx_vm mmx_iface mmx_modules
	vnx_base vnx_addons url_cpp llhttp
	vnxpasswd generate_passwd
	automy_basic_opencl
)

list(APPEND TOOL_FILES
	mmx_timelord_rewards
	tx_bench
)
foreach(APPFILE IN LISTS APP_FILES TOOL_FILES)
	set(ProductVersionFiles "ProductVersionFiles_${APPFILE}")
	generate_product_version(
		${ProductVersionFiles}
		NAME ${APPFILE}
		BUNDLE "MMX Node"
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
	COMMAND ${CMAKE_MAKE_PROGRAM} MMX_Node_GUI.sln -restore -m 
			/p:Configuration=Release
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
			$<TARGET_FILE_DIR:automy_basic_opencl>/OpenCL.dll
			$<TARGET_FILE_DIR:mmx_modules>/miniupnpc.dll
		DESTINATION ./ COMPONENT applications)

install(DIRECTORY kernel/ DESTINATION kernel COMPONENT applications)
install(DIRECTORY config/ DESTINATION config COMPONENT applications)
install(DIRECTORY www/ DESTINATION www COMPONENT applications)
install(DIRECTORY data/ DESTINATION data COMPONENT applications)
install(DIRECTORY scripts/win/ DESTINATION ./ COMPONENT applications)
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION ./ COMPONENT applications)

FetchContent_Declare(
	mmx_binaries
	GIT_REPOSITORY https://github.com/madMAx43v3r/mmx-binaries.git
	GIT_TAG e1fa7cb8b3daea99a3f35879919d3c983c7671e2
)
FetchContent_MakeAvailable(mmx_binaries)

set(MMX_CLASSIC_PLOTTER_PATH ${mmx_binaries_SOURCE_DIR}/mmx-cpu-plotter/windows/classic)
install(FILES ${MMX_CLASSIC_PLOTTER_PATH}/chia_plot/chia_plot.exe DESTINATION ./ RENAME mmx_plot.exe COMPONENT plotters)
install(FILES ${MMX_CLASSIC_PLOTTER_PATH}/chia_plot/chia_plot_k34.exe DESTINATION ./ RENAME mmx_plot_k34.exe COMPONENT plotters)
install(FILES ${MMX_CLASSIC_PLOTTER_PATH}/bladebit/bladebit.exe DESTINATION ./ RENAME mmx_bladebit.exe COMPONENT plotters)

set(MMX_PLOTTER_COMP_PATH ${mmx_binaries_SOURCE_DIR}/mmx-cpu-plotter/windows)
install(FILES ${MMX_PLOTTER_COMP_PATH}/chia_plot.exe DESTINATION ./ RENAME mmx_plot_comp.exe COMPONENT plotters)
install(FILES ${MMX_PLOTTER_COMP_PATH}/chia_plot_k34.exe DESTINATION ./ RENAME mmx_plot_k34_comp.exe COMPONENT plotters)

set(MMX_CUDA_PLOTTER_PATH ${mmx_binaries_SOURCE_DIR}/mmx-cuda-plotter/format-v2.4/windows)
foreach(KSIZE 30 31 32 33 34)
	install(FILES ${MMX_CUDA_PLOTTER_PATH}/cuda_plot_k${KSIZE}.exe DESTINATION ./ RENAME cuda_plot_k${KSIZE}.exe COMPONENT plotters)
endforeach()

set(MMX_PLOT_SINK_PATH ${mmx_binaries_SOURCE_DIR}/chia-plot-sink/windows)
install(DIRECTORY ${MMX_PLOT_SINK_PATH}/ DESTINATION ./ COMPONENT plotters)

set(CPACK_PACKAGE_NAME "MMX Node")
set(CPACK_PACKAGE_VENDOR "madMAx43v3r")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MMX is a blockchain written from scratch using Chia's Proof Of Space and a SHA256 VDF similar to Solana")
set(CPACK_RESOURCE_FILE_LICENSE ${PROJECT_SOURCE_DIR}/LICENSE)
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/madMAx43v3r/mmx-node")

set(CPACK_PACKAGE_INSTALL_DIRECTORY "MMX")

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

endif()