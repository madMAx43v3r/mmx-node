

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
 		${CMAKE_SOURCE_DIR}/scripts/win/ $<TARGET_FILE_DIR:mmx>)

if(NOT MMX_VERSION MATCHES "^v([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
	string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d")
	set(MMX_VERSION "v0.6.0.${BUILD_TIMESTAMP}")
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
set(MMX_ICON "${CMAKE_CURRENT_SOURCE_DIR}/mmx.ico")
set(MMX_FRIENDLY_STRING "MMX Node ${MMX_VERSION}")

list(APPEND APP_FILES
	mmx mmx_node mmx_farmer mmx_wallet mmx_timelord mmx_harvester
	mmx_db mmx_vm mmx_vm_iface mmx_iface mmx_modules mmx_chiapos
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
		DESTINATION ./ COMPONENT applications)

install(DIRECTORY kernel/ DESTINATION kernel COMPONENT applications)
install(DIRECTORY config/ DESTINATION config COMPONENT applications)
install(DIRECTORY www/ DESTINATION www COMPONENT applications)
install(DIRECTORY scripts/win/ DESTINATION ./ COMPONENT applications)
install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION ./ COMPONENT applications)


include(FetchContent)
FetchContent_Declare(plotter_k32
URL https://github.com/MMX-World/mmx-plotter/releases/download/1.1.8/chia_plot-1.1.8.exe
URL_HASH SHA256=EF00C99423CB9980CA0DC59CCE076E12BFE4DBA1ACC99E17FA9CFD163604FF77
DOWNLOAD_NO_EXTRACT true
)
FetchContent_MakeAvailable(plotter_k32)
set (MMX_PLOTTER_K32_PATH ${plotter_k32_SOURCE_DIR}/chia_plot-1.1.8.exe)

FetchContent_Declare(plotter_k34
URL https://github.com/MMX-World/mmx-plotter/releases/download/1.1.8/chia_plot_k34-1.1.8.exe
URL_HASH SHA256=3329661BCE509A08638E99352A9607C3ADEAB11DD4B462895FCBEFDC4F22231F
DOWNLOAD_NO_EXTRACT true
)
FetchContent_MakeAvailable(plotter_k34)
set (MMX_PLOTTER_K34_PATH ${plotter_k34_SOURCE_DIR}/chia_plot_k34-1.1.8.exe)

FetchContent_Declare(plotter_bladebit
URL https://github.com/MMX-World/bladebit/releases/download/v1.2.5/bladebit.exe
URL_HASH SHA256=BAAB6CB0493268CB8071E0D3FF57919F9C0E8D09D730686DFD550FBCD1FE9D0C
DOWNLOAD_NO_EXTRACT true
)
FetchContent_MakeAvailable(plotter_bladebit)
set (PLOTTER_BLADEBIT_PATH ${plotter_bladebit_SOURCE_DIR}/bladebit.exe)

# include(ExternalProject)
# ExternalProject_Add(mmx_plotter
#   GIT_REPOSITORY    https://github.com/MMX-World/mmx-plotter.git
#   GIT_TAG           master
#   SOURCE_DIR        "${CMAKE_BINARY_DIR}/mmx_plotter-src"
#   BINARY_DIR        "${CMAKE_BINARY_DIR}/mmx_plotter-build"
#   CONFIGURE_COMMAND ${CMAKE_COMMAND} -S <SOURCE_DIR> -B <BINARY_DIR>
#   BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config Release
#   INSTALL_COMMAND   ""
#   TEST_COMMAND      ""
# )
# ExternalProject_Get_Property(mmx_plotter BINARY_DIR)
# set (MMX_PLOTTER_K32_PATH ${BINARY_DIR}/Release/chia_plot.exe)
# set (MMX_PLOTTER_K34_PATH ${BINARY_DIR}/Release/chia_plot_k34.exe)

install(FILES ${MMX_PLOTTER_K32_PATH} DESTINATION ./ RENAME mmx_plot.exe COMPONENT plotters)
install(FILES ${MMX_PLOTTER_K34_PATH} DESTINATION ./ RENAME mmx_plot_k34.exe COMPONENT plotters)
install(FILES ${PLOTTER_BLADEBIT_PATH} DESTINATION ./ RENAME bladebit.exe COMPONENT plotters)

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

set(CPACK_NSIS_MENU_LINKS
"mmx_cmd.cmd" "MMX CMD"
#"run_node.cmd" "MMX Node"
#"run_light_node.cmd" "MMX Light Node"
#"http://localhost:11380/gui/" "MMX Node WebGUI"
"MmxGui.exe" "MMX Node"
)
set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)

set(CPACK_NSIS_MUI_ICON ${MMX_ICON})
set(CPACK_NSIS_INSTALLED_ICON_NAME "MmxGui.exe")

# Must be after the last CPACK macros
include(CPack)

endif()