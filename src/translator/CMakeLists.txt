add_library(bergamot-translator STATIC
    AbstractTranslationModel.cpp
    TranslationModel.cpp
    TranslationModelConfigToOptionsAdaptor.cpp

    # Following files added from browsermt/mts@nuke
    textops.cpp
    batch_translator.cpp 
    multifactor_priority.cpp 
    request.cpp 
    service.cpp
    batcher.cpp
    utils.cpp
    translation_result.cpp
)

# Replacement app for marian-decoder from browsermt/mts@nuke
add_executable(main main.cpp)
set_target_properties(main PROPERTIES OUTPUT bergamot-cli RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_compile_options(main PUBLIC ${ALL_WARNINGS})
set(EXECUTABLES ${EXECUTABLES} main)
target_link_libraries(main bergamot-translator marian ${MARIAN_CUDA_LIB} ${EXT_LIBS} ssplit pcrecpp.a pcre.a)

target_link_libraries(bergamot-translator marian)
target_include_directories(bergamot-translator
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}
    PUBLIC ${CMAKE_SOURCE_DIR}/src)

