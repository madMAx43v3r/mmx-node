
find_package(Git)

execute_process(
  COMMAND ${GIT_EXECUTABLE} describe --tags
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_BUILD_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
  COMMAND ${GIT_EXECUTABLE} log -1 --format=%H
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_BUILD_COMMIT
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# NOTE: Sample results for 'git' commands above:
# 'git describe --tags'
# - "v0.13.9"
# - "v0.13.9-8-gd7d82d79"
# - "fatal: No names found, cannot describe anything."
# 'git log -1 --format=%H'
# - "a8390d2bd45732bde2aa18ad58ef93da92934c47"

# NOTE: 'version' / only accept 'v0.13.9' or 'v0.13.9-8-gd7d82d79' format (max 80 chars), else fallback to 'v0.0.0.20250117'
# NOTE: 'commit' / only accept SHA1 format (lowercase, 40 chars), else fallback to '0000000000000000000000000000000000000000'

string(LENGTH "${GIT_BUILD_VERSION}" GIT_BUILD_VERSION_LEN)
if(NOT GIT_BUILD_VERSION MATCHES "^v[0-9]+\\.[0-9]+\\.[0-9]+$|^v[0-9]+\\.[0-9]+\\.[0-9]+\\-[0-9]+\\-g[0-9a-f]+$" OR GIT_BUILD_VERSION_LEN GREATER 80)
  string(TIMESTAMP GIT_BUILD_TIMESTAMP "%Y%m%d")
  set(GIT_BUILD_VERSION "v0.0.0.${GIT_BUILD_TIMESTAMP}")
endif()

string(LENGTH "${GIT_BUILD_COMMIT}" GIT_BUILD_COMMIT_LEN)
if(NOT GIT_BUILD_COMMIT MATCHES "^[0-9a-f]+$" OR NOT GIT_BUILD_COMMIT_LEN EQUAL 40)
  set(GIT_BUILD_COMMIT "0000000000000000000000000000000000000000")
endif()

message(STATUS "GIT_BUILD_VERSION=${GIT_BUILD_VERSION}")
message(STATUS "GIT_BUILD_COMMIT=${GIT_BUILD_COMMIT}")

# NOTE: Write build version to config area (for API/WAPI)

file(WRITE config/default/build.json "{\n\t\"version\": \"${GIT_BUILD_VERSION}\",\n\t\"commit\": \"${GIT_BUILD_COMMIT}\"\n}\n")
