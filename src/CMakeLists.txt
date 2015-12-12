include_directories(
  ${PROJECT_SOURCE_DIR}/includes/
  ${PROJECT_BINARY_DIR}/includes/
  ${PROJECT_SOURCE_DIR}/src 
  ${PROJECT_BINARY_DIR}/src
  )

set(LIBRARIES)

### MANUAL_INCLUDE_DIRS is useful to force ides to find some includes (useless for compilation) ###
### To be used by editing the cache manually
include_directories(${MANUAL_INCLUDE_DIRS})

# Check dependencies
include_directories(${PROJECT_SOURCE_DIR}/external/json_backbone/include)
#include(CheckIncludeFileCxx)
#check_include_file_cxx(json_backbone/container.hpp JSON_BACKBONE_FOUND)
#if (NOT JSON_BACKBONE_FOUND)
#  message(FATAL_ERROR "The json_bacbone dependency has not been found.")
#endif()

Find_Package(Threads REQUIRED)

# Rapidjson support
set(RAPIDJSON_INCLUDEDIR ${PROJECT_SOURCE_DIR}/external/rapidjson/include)
Find_Package(rapidjson REQUIRED)

# Boost support
FIND_PACKAGE(Boost COMPONENTS program_options REQUIRED)
if(Boost_FOUND)
include_directories(${Boost_INCLUDE_DIRS})
if(NOT MSVC)
    set(LIBRARIES "${LIBRARIES}" "${Boost_LIBRARIES}")
  else()
    link_directories(${Boost_LIBRARY_DIRS})
  endif()
endif()

### Browsing source files ###
file(GLOB lib_srcs *.cc)
list(REMOVE_ITEM lib_srcs "${PROJECT_SOURCE_DIR}/src/main.cc")

### Exe ###
add_library(${PROJECT_NAME}_OBJECTS OBJECT ${lib_srcs})
add_executable(${PROJECT_NAME} main.cc $<TARGET_OBJECTS:${PROJECT_NAME}_OBJECTS>)

### Linking ###
target_link_libraries(${PROJECT_NAME} ${LIBRARIES})
