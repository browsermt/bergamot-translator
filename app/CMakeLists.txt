add_executable(bergamot-translator-app main.cpp)
target_link_libraries(bergamot-translator-app PRIVATE bergamot-translator)

# Replacement app for marian-decoder from browsermt/mts@nuke
add_executable(main main-mts.cpp)
set_target_properties(main PROPERTIES OUTPUT bergamot-cli RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_compile_options(main PUBLIC ${ALL_WARNINGS})
set(EXECUTABLES ${EXECUTABLES} main)
target_link_libraries(main bergamot-translator marian ${MARIAN_CUDA_LIB} ${EXT_LIBS} ssplit pcrecpp.a pcre.a)
