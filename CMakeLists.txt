cmake_minimum_required(VERSION 3.5.1)

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW) # CMake 3.12
endif ()

project(bergamot_translator CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_ARCH native CACHE STRING "Compile for this CPU architecture.")

add_subdirectory(src)