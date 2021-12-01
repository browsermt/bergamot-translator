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
  set(TEST_BINARIES async blocking intgemm-resolve wasm)
  foreach(binary ${TEST_BINARIES})
      add_executable("${binary}" "${binary}.cpp")
      target_link_libraries("${binary}" bergamot-translator)
      set_target_properties("${binary}" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/")
  endforeach(binary)

endif(NOT MSVC)

