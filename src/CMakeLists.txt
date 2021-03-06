include(ExternalProject)
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
#include(CheckIncludeFile)
#check_include_file(${PROJECT_SOURCE_DIR}/external/json_backbone/include/json_backbone/container.hpp JSON_BACKBONE_FOUND)
#if (NOT JSON_BACKBONE_FOUND)
#  message(FATAL_ERROR "The json_bacbone dependency has not been found.")
#endif()

Find_Package(Threads REQUIRED)
set(LIBRARIES "${LIBRARIES}" ${CMAKE_THREAD_LIBS_INIT})

# MongoDB driver
ExternalProject_Add(libcppformat
  PREFIX ${PROJECT_BINARY_DIR}
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/cppformat
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${PROJECT_BINARY_DIR}
  )
ExternalProject_Add(libbson
  PREFIX ${PROJECT_BINARY_DIR}
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/libbson
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${PROJECT_BINARY_DIR}
  )
ExternalProject_Add(libmongoc
  DEPENDS libbson
  PREFIX ${PROJECT_BINARY_DIR}
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/libmongoc
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${PROJECT_BINARY_DIR}
  )
ExternalProject_Add(mongodb-cxx-driver
  DEPENDS libbson libmongoc
  PREFIX ${PROJECT_BINARY_DIR}
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/mongo-cxx-driver
  CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:PATH=${PROJECT_BINARY_DIR} -DCMAKE_PREFIX_PATH:PATH=${PROJECT_BINARY_DIR}
  )

include_directories(
  ${PROJECT_BINARY_DIR}/include
  ${PROJECT_BINARY_DIR}/include/bsoncxx/v_noabi
  ${PROJECT_BINARY_DIR}/include/libbson-1.0
  ${PROJECT_BINARY_DIR}/include/libmongoc-1.0
  ${PROJECT_BINARY_DIR}/include/mongocxx/v_noabi
  )
link_directories(${PROJECT_BINARY_DIR}/lib)
set(LIBRARIES "${LIBRARIES}" bson-1.0 bsoncxx mongoc-1.0 mongocxx cppformat)

# Boost support
FIND_PACKAGE(Boost COMPONENTS program_options filesystem system REQUIRED)
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
add_dependencies(${PROJECT_NAME}_OBJECTS mongodb-cxx-driver)
add_executable(${PROJECT_NAME} main.cc $<TARGET_OBJECTS:${PROJECT_NAME}_OBJECTS>)
add_dependencies(${PROJECT_NAME} mongodb-cxx-driver libcppformat)

### Linking ###
target_link_libraries(${PROJECT_NAME} ${LIBRARIES})

### Tests ###
add_subdirectory(test)
