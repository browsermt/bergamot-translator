# Unit tests
set(UNIT_TESTS
    annotation_tests
)

# Annotation Test
add_executable("run_annotation_tests" run_tests.cpp "annotation_tests.cpp" "../translator/sentence_ranges.cpp")
target_include_directories("run_annotation_tests" PRIVATE ${CATCH_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/src")
add_test(NAME annotation_tests COMMAND "run_annotation_tests")

# foreach(test ${UNIT_TESTS})
#   add_executable("run_${test}" run_tests.cpp "${test}.cpp")
#   # message("CMAKE source dir" ${CMAKE_SOURCE_DIR}/src)
#   target_include_directories("run_${test}" PRIVATE ${CATCH_INCLUDE_DIR} "${CMAKE_SOURCE_DIR}/src")
# 
# 
#   if(msvc)
#     # disable c4305: truncation from 'double' to '_ty'
#     target_compile_options("run_${test}" public /wd4305)
#   endif(msvc)
# 
#   add_test(NAME ${test} COMMAND "run_${test}")
# endforeach(test)
