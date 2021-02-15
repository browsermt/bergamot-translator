cmake_minimum_required(VERSION 3.5.1)

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW) # CMake 3.12
endif ()

project(bergamot_translator CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Project specific cmake options
option(COMPILE_WASM "Compile for WASM" OFF)
option(COMPILE_THREAD_VARIANT "Compile with thread support" OFF)
SET(PACKAGE_DIR "" CACHE STRING "Directory including all the files to be packaged (pre-loaded) in wasm builds")

# Set marian (3rd party submodule) cmake options to compile for this project
SET(COMPILE_CUDA OFF CACHE BOOL "Compile GPU version")
SET(USE_SENTENCEPIECE ON CACHE BOOL "Download and compile SentencePiece")
SET(USE_STATIC_LIBS ON CACHE BOOL "Link statically against non-system libs")
SET(USE_MKL OFF CACHE BOOL "Compile with MKL support")
SET(COMPILE_DECODER_ONLY ON CACHE BOOL "Compile marian-decoder only")
SET(COMPILE_WITH_PTHREADS OFF CACHE BOOL "Compile with pthreads support")
SET(USE_WASM_COMPATIBLE_BLAS ON CACHE BOOL "Compile with a WASM compatible blas for decoder only builds")
SET(COMPILE_LIBRARY_ONLY ON CACHE BOOL "Build only the Marian library and exclude all executables.")
if(COMPILE_WASM)
  # Set WORMHOLE to ON for marian whenever compiling for wasm platform
  SET(WORMHOLE ON CACHE BOOL "Use WASM wormhole in intgemm https://bugzilla.mozilla.org/show_bug.cgi?id=1672160")
endif()

execute_process(COMMAND git submodule update --init --recursive --no-fetch
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT COMPILE_WASM)
  # Set BUILD_ARCH to native only while compiling for non wasm platform
  set(BUILD_ARCH native CACHE STRING "Compile for this CPU architecture.")
endif()

if(COMPILE_WASM)
  list(APPEND WASM_COMPILE_FLAGS -pthread -O3 -g2 -fPIC -mssse3 -msimd128)
  list(APPEND WASM_COMPILE_FLAGS "SHELL:-s WASM=1" "SHELL:-s ASSERTIONS=1" "SHELL:-s DISABLE_EXCEPTION_CATCHING=0" "SHELL:-s LLD_REPORT_UNDEFINED" "SHELL:-s FORCE_FILESYSTEM=1" "SHELL:-s ALLOW_MEMORY_GROWTH=1")
  list(APPEND WASM_COMPILE_FLAGS -Wno-error=pthreads-mem-growth)
endif(COMPILE_WASM)

add_subdirectory(3rd_party)
add_subdirectory(src)
if(NOT COMPILE_WASM)
  add_subdirectory(app)
endif()
if(COMPILE_WASM)
  add_subdirectory(wasm)
endif(COMPILE_WASM)
