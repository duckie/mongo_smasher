cmake_minimum_required (VERSION 3.1)
project (mongo_smasher)
enable_testing()

set(CMAKE_VERBOSE_MAKEFILE OFF)

# Referencing local modules
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# C++11 is a requirement here
include(CheckCXXCompilerFlag)
if (NOT MSVC)
  check_cxx_compiler_flag(-std=c++14 HAVE_STD_CPP11_FLAG)
  if (HAVE_STD_CPP11_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
  else()
    message(FATAL_ERROR "Your compiler must support at least C++14 to compile ${PROJECT_NAME}.")
  endif()
endif()

# Trace compilation
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
configure_file(${PROJECT_SOURCE_DIR}/cmake/BuildConfig.json.in ${PROJECT_SOURCE_DIR}/BuildConfig.json)

# Main source directory
enable_testing()
add_subdirectory (src)
