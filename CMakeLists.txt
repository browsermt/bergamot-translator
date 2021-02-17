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
option(USE_MKL "Compile with MKL support" ON)

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

add_subdirectory(3rd_party)
add_subdirectory(src)
add_subdirectory(app)


