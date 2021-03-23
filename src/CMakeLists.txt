add_subdirectory(translator)

if(COMPILE_BERGAMOT_TESTS)
  # Catch currently comes from marian sources.
  add_subdirectory(tests)
endif(COMPILE_BERGAMOT_TESTS)

