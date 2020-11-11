cmake_minimum_required(VERSION 3.5.1)

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW) # CMake 3.12
endif ()

project(bergamot_translator CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_ARCH native CACHE STRING "Compile for this CPU architecture.")

# Custom CMake options to compile marian (a 3rd party submodule) for this project
option(COMPILE_CUDA "Compile GPU version" OFF)
option(USE_SENTENCEPIECE "Download and compile SentencePiece" ON)
option(USE_STATIC_LIBS "Link statically against non-system libs" ON)

add_subdirectory(3rd_party)
add_subdirectory(src)
