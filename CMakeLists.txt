cmake_minimum_required(VERSION 3.5.1)
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

if (POLICY CMP0074)
  cmake_policy(SET CMP0074 NEW) # CMake 3.12
endif ()

if (POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

project(bergamot_translator CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generate a compile_commands.json in the build directory. The compile commands allow
# code editors to understand the build process and provide static analysis of the code.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Note that with CMake MSVC build, the option CMAKE_BUILD_TYPE is automatically derived from the key
# 'configurationType' in CMakeSettings.json configurations
if(NOT CMAKE_BUILD_TYPE)
  message(WARNING "CMAKE_BUILD_TYPE not set; setting to Release")
  set(CMAKE_BUILD_TYPE "Release")
endif()

if(NOT COMPILE_WASM)
  # Setting BUILD_ARCH to native invokes CPU intrinsic detection logic below.
  # Prevent invoking that logic for WASM builds.
  set(BUILD_ARCH native CACHE STRING "Compile for this CPU architecture.")

  # Unfortunately MSVC supports a limited subset of BUILD_ARCH flags. Instead try to guess
  # what architecture we can compile to reading BUILD_ARCH and mapping it to MSVC values
  # references: https://clang.llvm.org/docs/UsersManual.html https://gcc.gnu.org/onlinedocs/gcc/x86-Options.html https://gcc.gnu.org/onlinedocs/gcc-4.8.5/gcc/i386-and-x86-64-Options.html
  # https://docs.microsoft.com/en-us/cpp/build/reference/arch-x86?redirectedfrom=MSDN&amp;amp;view=vs-2019&view=msvc-170 https://devblogs.microsoft.com/oldnewthing/20201026-00/?p=104397
  # This is by no means an exhaustive list but should match the most common flags Linux programmers expect to parse to MSVC
  if(MSVC)
    if(BUILD_ARCH STREQUAL "native") # avx2 is good default for native. Very few desktop systems support avx512
      set(MSVC_BUILD_ARCH "/arch:AVX2")
    elseif(BUILD_ARCH STREQUAL "skylake-avx512" OR BUILD_ARCH STREQUAL "cannonlake" OR BUILD_ARCH STREQUAL "x86-64-v4" OR BUILD_ARCH STREQUAL "tigerlake" OR BUILD_ARCH STREQUAL "cooperlake" OR BUILD_ARCH STREQUAL "cascadelake")
      set(MSVC_BUILD_ARCH "/arch:AVX512")
    elseif(BUILD_ARCH STREQUAL "core-avx2" OR BUILD_ARCH STREQUAL "haswell" OR BUILD_ARCH STREQUAL "x86-64-v3" OR BUILD_ARCH STREQUAL "broadwell" OR BUILD_ARCH STREQUAL "skylake")
      set(MSVC_BUILD_ARCH "/arch:AVX2")
    elseif(BUILD_ARCH STREQUAL "sandybridge" OR BUILD_ARCH STREQUAL "corei7-avx" OR BUILD_ARCH STREQUAL "core-avx-i" OR BUILD_ARCH STREQUAL "ivybridge")
      set(MSVC_BUILD_ARCH "/arch:AVX")
    elseif(BUILD_ARCH STREQUAL "nehalem" OR BUILD_ARCH STREQUAL "westmere" OR BUILD_ARCH STREQUAL "x86-64-v2" OR BUILD_ARCH STREQUAL "corei7" OR BUILD_ARCH STREQUAL "core2")
      set(MSVC_BUILD_ARCH "/arch:SSE2") # This is MSVC default. We won't go down to SSE because we don't support that hardware at all with intgemm. Marian recommends to only go down to SSE4.1 at most
    else()
      message(WARNING "Unknown BUILD_ARCH ${BUILD_ARCH} provided. Default to SSE2 for Windows build")
      set(MSVC_BUILD_ARCH "/arch:SSE2")
    endif()
  endif(MSVC)
endif()

#MSVC can't seem to pick up correct flags otherwise:
if(MSVC)
  add_definitions(-DUSE_SSE2=1) # Supposed to fix something in the sse_mathfun.h but not sure it does
  set(INTRINSICS ${MSVC_BUILD_ARCH}) # ARCH we're targetting on win32. @TODO variable
  
  set(CMAKE_CXX_FLAGS           "/EHsc /DWIN32 /D_WINDOWS /DUNICODE /D_UNICODE /D_CRT_NONSTDC_NO_WARNINGS /D_CRT_SECURE_NO_WARNINGS /bigobj")
  set(CMAKE_CXX_FLAGS_RELEASE   "${CMAKE_CXX_FLAGS} /MT /O2 ${INTRINSICS} /MP /GL /DNDEBUG")
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
cmake_dependent_option(USE_WASM_COMPATIBLE_SOURCE "Use wasm compatible sources" OFF "NOT COMPILE_WASM" ON)

# WASM disables a million libraries, which also includes the unit test-library.
cmake_dependent_option(COMPILE_UNIT_TESTS "Compile unit tests" OFF "USE_WASM_COMPATIBLE_SOURCE" ON)
option(COMPILE_TESTS "Compile bergamot-tests" OFF)
cmake_dependent_option(ENABLE_CACHE_STATS "Enable stats on cache" ON "COMPILE_TESTS" OFF)


# Set 3rd party submodule specific cmake options for this project
SET(COMPILE_CUDA OFF CACHE BOOL "Compile GPU version")
SET(USE_SENTENCEPIECE ON CACHE BOOL "Download and compile SentencePiece")
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

# Project versioning
include(GetVersionFromFile)
message(STATUS "Project name: ${PROJECT_NAME}")
message(STATUS "Project version: ${PROJECT_VERSION_STRING_FULL}")

if(COMPILE_WASM)
  # See https://github.com/emscripten-core/emscripten/blob/main/src/settings.js
  list(APPEND WASM_COMPILE_FLAGS
    -O3
    # Preserve whitespaces in JS even for release builds; this doesn't increase wasm binary size
    $<$<CONFIG:Release>:-g1>
    # Relevant Debug info only for release with debug builds as this increases wasm binary size
    $<$<CONFIG:RelWithDebInfo>:-g2>
    -fPIC
    -mssse3
    -msimd128
    # -fno-exceptions # Can't do that because spdlog uses exceptions
    -sDISABLE_EXCEPTION_CATCHING=1
    -sSTRICT=1
  )
  list(APPEND WASM_LINK_FLAGS
    -O3
    # Preserve whitespaces in JS even for release builds; this doesn't increase wasm binary size
    $<$<CONFIG:Release>:-g1>
    # Relevant Debug info only for release with debug builds as this increases wasm binary size
    $<$<CONFIG:RelWithDebInfo>:-g2>
    -lembind
    # Save some code, and some speed
    -sASSERTIONS=0
    -sDISABLE_EXCEPTION_CATCHING=1
    # the intgemm functions we call will be undefined since these are linked at
    # runtime by our own javascript.
    -sLLD_REPORT_UNDEFINED
    -sERROR_ON_UNDEFINED_SYMBOLS=0
    # Cause we can!
    -sSTRICT=1
    # You know we need it
    -sALLOW_MEMORY_GROWTH=1
    -sENVIRONMENT=web,worker
    # No need to call main(), there's nothing there.
    -sINVOKE_RUN=0
    # No need for filesystem code in the generated Javascript
    -sFILESYSTEM=0
    # If you turn this on, it will mangle names which makes the dynamic linking hard.
    -sDECLARE_ASM_MODULE_EXPORTS=0
    # Export all of the intgemm functions in case we need to fall back to using the embedded intgemm
    -sEXPORTED_FUNCTIONS=[_int8PrepareAFallback,_int8PrepareBFallback,_int8PrepareBFromTransposedFallback,_int8PrepareBFromQuantizedTransposedFallback,_int8PrepareBiasFallback,_int8MultiplyAndAddBiasFallback,_int8SelectColumnsOfBFallback]
    # Necessary for mozintgemm linking. This prepares the `wasmMemory` variable ahead of time as
    # opposed to delegating that task to the wasm binary itself. This way we can link MozIntGEMM
    # module to the same memory as the main bergamot-translator module.
    -sIMPORTED_MEMORY=1
    # Dynamic execution is either frowned upon or blocked inside browser extensions
    -sDYNAMIC_EXECUTION=0
  )
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

option(COMPILE_PYTHON "Compile python bindings. Intended to be activated with setup.py" OFF)
if(COMPILE_PYTHON)
  add_subdirectory(bindings/python)
endif(COMPILE_PYTHON)

