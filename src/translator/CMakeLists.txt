add_library(bergamot-translator STATIC
    AbstractTranslationModel.cpp
    TranslationModel.cpp

    # Following files added from browsermt/mts@nuke
    textops.cpp
    batch_translator.cpp 
    multifactor_priority.cpp 
    request.cpp 
    service.cpp
    batcher.cpp
    translation_result.cpp
)
if (COMPILE_DECODER_ONLY)
  # A dirty hack because of marian's bad cmake practices
  target_compile_definitions(bergamot-translator PUBLIC DECODER_ONLY)
endif()

if(COMPILE_WASM)
  # A dirty hack because of marian's bad cmake practices
  target_compile_definitions(bergamot-translator PUBLIC USE_SSE2 WASM)
endif(COMPILE_WASM)

target_link_libraries(bergamot-translator marian ssplit)

target_include_directories(bergamot-translator
    PRIVATE ${CMAKE_SOURCE_DIR}
    PUBLIC ${CMAKE_SOURCE_DIR}/src)

