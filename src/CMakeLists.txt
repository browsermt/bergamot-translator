add_subdirectory(translator)

if(COMPILE_BERGAMOT_TESTS)
  set(CATCH_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/3rd_party/marian-dev/src/3rd_party" "${CMAKE_SOURCE_DIR}/3rd_party/marian-dev/src/")
  add_library(Catch INTERFACE)
  target_include_directories(Catch INTERFACE ${CATCH_INCLUDE_DIR})
  add_subdirectory(tests)
endif(COMPILE_BERGAMOT_TESTS)

