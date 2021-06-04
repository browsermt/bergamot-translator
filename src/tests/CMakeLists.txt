# Unit tests
if (COMPILE_UNIT_TESTS)
    add_subdirectory(units)
endif (COMPILE_UNIT_TESTS)

if(NOT MSVC)
  # Testing apps
  set(APP_TESTS)
  add_executable("bergamot-test" "cli.cpp" "apps.cpp")
  
  if(CUDA_FOUND)
    target_link_libraries("bergamot-test" ${EXT_LIBS} marian ${EXT_LIBS} marian_cuda ${EXT_LIBS} bergamot-translator)
  else(CUDA_FOUND)
    target_link_libraries("bergamot-test" marian ${EXT_LIBS} bergamot-translator)
  endif(CUDA_FOUND)
  
  set_target_properties("bergamot-test" PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
endif(NOT MSVC)

