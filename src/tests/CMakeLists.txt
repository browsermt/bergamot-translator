# Unit tests

# Include Catch explicitly from marian.
set(CATCH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rd_party/marian-dev/3rd-party)
add_library(Catch INTERFACE)
target_include_directories(Catch INTERFACE ${CATCH_INCLUDE_DIR})

if (COMPILE_UNIT_TESTS)
    add_subdirectory(units)
endif (COMPILE_UNIT_TESTS)

if(NOT MSVC)
  # Testing apps
  set(APP_TESTS)
  add_executable("bergamot-test" "cli.cpp" "apps.cpp")
  
  if(CUDA_FOUND)
    target_link_libraries("bergamot-test" bergamot-translator)
  else(CUDA_FOUND)
    target_link_libraries("bergamot-test" bergamot-translator)
  endif(CUDA_FOUND)
  
  set_target_properties("bergamot-test" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif(NOT MSVC)

