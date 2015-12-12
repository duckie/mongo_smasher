cmake_minimum_required (VERSION 2.8)
project (mongo_smasher)
enable_testing()

set(CMAKE_VERBOSE_MAKEFILE OFF)

# Referencing local modules
set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# C++11 is a requirement here
include(CheckCXXCompilerFlag)
if (NOT MSVC)
  check_cxx_compiler_flag(-std=c++11 HAVE_STD_CPP11_FLAG)
  if (HAVE_STD_CPP11_FLAG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  else()
    message(FATAL_ERROR "Your compiler must support at least C++11 to compile ${PROJECT_NAME}.")
  endif()
endif()

# Main source directory
add_subdirectory (src)
#add_subdirectory (test)

