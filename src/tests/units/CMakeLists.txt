# Unit tests
set(UNIT_TESTS
    annotation_tests
)

foreach(test ${UNIT_TESTS})
  add_executable("run_${test}" run_tests.cpp "${test}.cpp")
  target_include_directories("run_${test}" PRIVATE ${CATCH_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/src")

  if(CUDA_FOUND)
    target_link_libraries("run_${test}" ${EXT_LIBS} marian ${EXT_LIBS} marian_cuda ${EXT_LIBS} Catch bergamot-translator)
  else(CUDA_FOUND)
    target_link_libraries("run_${test}" marian ${EXT_LIBS} Catch bergamot-translator)
  endif(CUDA_FOUND)

  if(msvc)
    # disable c4305: truncation from 'double' to '_ty'
    target_compile_options("run_${test}" public /wd4305)
  endif(msvc)

  add_test(NAME ${test} COMMAND "run_${test}")
endforeach(test)
