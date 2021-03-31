cmake_minimum_required(VERSION 3.5.1)

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW) # CMake 3.12
endif ()

project(bergamot_translator CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CMakeDependentOption)

# Project specific cmake options
option(COMPILE_WASM "Compile for WASM" OFF)
option(USE_WASM_COMPATIBLE_SOURCE "Use wasm compatible sources" ON)
option(COMPILE_TESTS "Compile bergamot-tests" OFF)

SET(PACKAGE_DIR "" CACHE STRING "Directory including all the files to be packaged (pre-loaded) in wasm builds")

# Set 3rd party submodule specific cmake options for this project
SET(COMPILE_CUDA OFF CACHE BOOL "Compile GPU version")
SET(USE_SENTENCEPIECE ON CACHE BOOL "Download and compile SentencePiece")
SET(USE_STATIC_LIBS ON CACHE BOOL "Link statically against non-system libs")
if (USE_WASM_COMPATIBLE_SOURCE)
  SET(COMPILE_LIBRARY_ONLY ON CACHE BOOL "Build only the Marian library and exclude all executables.")
  SET(USE_MKL OFF CACHE BOOL "Compile with MKL support")
  # # Setting the ssplit-cpp submodule specific cmake options for wasm
  SET(USE_INTERNAL_PCRE2 ON CACHE BOOL "Use internal PCRE2 instead of system PCRE2")
endif()

# Documentation: https://cliutils.gitlab.io/modern-cmake/chapters/projects/submodule.html
# Ensures the submodules are set correctly during a build.
find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
# Update submodules as needed
    option(GIT_SUBMODULE "Check submodules during build" ON)
    if(GIT_SUBMODULE)
        message(STATUS "Submodule update")
        execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        RESULT_VARIABLE GIT_SUBMOD_RESULT)
        if(NOT GIT_SUBMOD_RESULT EQUAL "0")
            message(FATAL_ERROR "git submodule update --init failed with ${GIT_SUBMOD_RESULT}, please checkout submodules")
        endif()
    endif()
endif()

if(NOT COMPILE_WASM)
  # Set BUILD_ARCH to native only while compiling for non wasm platform
  set(BUILD_ARCH native CACHE STRING "Compile for this CPU architecture.")
endif()

if(COMPILE_WASM)
  set(WORMHOLE ON CACHE BOOL "Use WASM wormhole in intgemm https://bugzilla.mozilla.org/show_bug.cgi?id=1672160")
  list(APPEND WASM_COMPILE_FLAGS -pthread -O3 -g2 -fPIC -mssse3 -msimd128)
  list(APPEND WASM_COMPILE_FLAGS "SHELL:-s WASM=1" "SHELL:-s ASSERTIONS=0" "SHELL:-s DISABLE_EXCEPTION_CATCHING=1" "SHELL:-s LLD_REPORT_UNDEFINED" "SHELL:-s FORCE_FILESYSTEM=1" "SHELL:-s ALLOW_MEMORY_GROWTH=1")
  list(APPEND WASM_COMPILE_FLAGS -Wno-error=pthreads-mem-growth)
endif(COMPILE_WASM)

# Needs to be enabled before including the folder containing tests (src/tests)
if(COMPILE_TESTS)
    enable_testing()
endif(COMPILE_TESTS)

add_subdirectory(3rd_party)
add_subdirectory(src)

if(COMPILE_WASM)
  add_subdirectory(wasm)
else()
  add_subdirectory(app)
endif(COMPILE_WASM)

