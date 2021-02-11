cmake_minimum_required(VERSION 3.5.1)

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW) # CMake 3.12
endif ()

project(bergamot_translator CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Project specific cmake options
option(COMPILE_WASM "Compile for WASM" OFF)

# Custom CMake options to compile marian (a 3rd party submodule) for this project
option(COMPILE_CUDA "Compile GPU version" OFF)
option(USE_SENTENCEPIECE "Download and compile SentencePiece" ON)
option(USE_STATIC_LIBS "Link statically against non-system libs" ON)
option(USE_MKL "Compile with MKL support" OFF)
option(COMPILE_DECODER_ONLY "Compile marian-decoder only" ON)
option(COMPILE_WITH_PTHREADS "Compile with pthreads support" OFF)
option(USE_WASM_COMPATIBLE_BLAS "Compile with a WASM compatible blas for decoder only builds" ON)

execute_process(COMMAND git submodule update --init --recursive --no-fetch
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT COMPILE_WASM)
  # Set BUILD_ARCH to native only while compiling for non wasm platform
  set(BUILD_ARCH native CACHE STRING "Compile for this CPU architecture.")
endif()

if(COMPILE_WASM)
  add_compile_options(-pthread -O3 -g2 -fPIC -mssse3 -msimd128)
  add_compile_options("SHELL:-s WASM=1" "SHELL:-s ASSERTIONS=1" "SHELL:-s DISABLE_EXCEPTION_CATCHING=0" "SHELL:-s LLD_REPORT_UNDEFINED" "SHELL:-s FORCE_FILESYSTEM=1" "SHELL:-s ALLOW_MEMORY_GROWTH=1")
  add_compile_options(-Wno-error=pthreads-mem-growth)
endif(COMPILE_WASM)

add_subdirectory(3rd_party)
add_subdirectory(src)
if(NOT COMPILE_WASM)
  add_subdirectory(app)
endif()
