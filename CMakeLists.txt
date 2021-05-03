cmake_minimum_required(VERSION 3.5.1)

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW) # CMake 3.12
endif ()

project(bergamot_translator CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Note that with CMake MSVC build, the option CMAKE_BUILD_TYPE is automatically derived from the key
# 'configurationType' in CMakeSettings.json configurations
if(NOT CMAKE_BUILD_TYPE)
  message(WARNING "CMAKE_BUILD_TYPE not set; setting to Release")
  set(CMAKE_BUILD_TYPE "Release")
endif()
#MSVC can't seem to pick up correct flags otherwise:
if(MSVC)
  add_definitions(-DUSE_SSE2=1) # Supposed to fix something in the sse_mathfun.h but not sure it does
  set(INTRINSICS "/arch:AVX2") # ARCH we're targetting on win32. @TODO variable
  
  set(CMAKE_CXX_FLAGS           "/EHsc /DWIN32 /D_WINDOWS /DUNICODE /D_UNICODE /D_CRT_NONSTDC_NO_WARNINGS /D_CRT_SECURE_NO_WARNINGS /bigobj")
  set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS} /MT /O2 ${INTRINSICS} /Zi /MP /GL /DNDEBUG")
  set(CMAKE_CXX_FLAGS_DEBUG     "${CMAKE_CXX_FLAGS} /MTd /Od /Ob0 ${INTRINSICS} /RTC1 /Zi /D_DEBUG")

  # ignores warning LNK4049: locally defined symbol free imported - this comes from zlib
  set(CMAKE_EXE_LINKER_FLAGS         "${CMAKE_EXE_LINKER_FLAGS} /DEBUG /LTCG:incremental /INCREMENTAL:NO /ignore:4049")
  set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRT")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG   "${CMAKE_EXE_LINKER_FLAGS} /NODEFAULTLIB:MSVCRTD")
  set(CMAKE_STATIC_LINKER_FLAGS      "${CMAKE_STATIC_LINKER_FLAGS} /LTCG:incremental")
endif(MSVC)

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
SET(SSPLIT_COMPILE_LIBRARY_ONLY ON CACHE BOOL "Do not compile ssplit tests")
if (USE_WASM_COMPATIBLE_SOURCE)
  SET(COMPILE_LIBRARY_ONLY ON CACHE BOOL "Build only the Marian library and exclude all executables.")
  SET(USE_MKL OFF CACHE BOOL "Compile with MKL support")
  # # Setting the ssplit-cpp submodule specific cmake options for wasm
  SET(SSPLIT_USE_INTERNAL_PCRE2 ON CACHE BOOL "Use internal PCRE2 instead of system PCRE2")
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

