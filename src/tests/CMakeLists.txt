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
  add_executable("bergamot-test-native" "native-cli.cpp" "apps.cpp")
  
  if(CUDA_FOUND)
    target_link_libraries("bergamot-test-native" bergamot-translator)
  else(CUDA_FOUND)
    target_link_libraries("bergamot-test-native" bergamot-translator)
  endif(CUDA_FOUND)
  
  set_target_properties("bergamot-test-native" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

  add_executable("bergamot-test-wasm" "wasm-cli.cpp" "apps.cpp")
  
  if(CUDA_FOUND)
    target_link_libraries("bergamot-test-wasm" bergamot-translator)
  else(CUDA_FOUND)
    target_link_libraries("bergamot-test-wasm" bergamot-translator)
  endif(CUDA_FOUND)
  
  set_target_properties("bergamot-test-wasm" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")

  # Adding an intgemm_resolve cmdline
  add_executable(intgemm-resolve intgemm_resolve.cpp)
  target_link_libraries(intgemm-resolve PRIVATE bergamot-translator)
  set_target_properties(intgemm-resolve PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif(NOT MSVC)

