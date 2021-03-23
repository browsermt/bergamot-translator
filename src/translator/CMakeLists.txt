add_library(bergamot-translator STATIC
    AbstractTranslationModel.cpp
    TranslationModel.cpp

    byteArrayExample.cpp
    text_processor.cpp
    sentence_splitter.cpp
    batch_translator.cpp 
    multifactor_priority.cpp 
    request.cpp 
    batcher.cpp
    response.cpp
    batch.cpp
    sentence_ranges.cpp
    service.cpp
)
if (COMPILE_DECODER_ONLY)
  # A dirty hack because of marian's bad cmake practices
  target_compile_definitions(bergamot-translator PUBLIC DECODER_ONLY)
endif()

if(COMPILE_WASM)
  # A dirty hack because of marian's bad cmake practices
  target_compile_definitions(bergamot-translator PUBLIC USE_SSE2 WASM)
  # Enable code that is required for generating JS bindings
  target_compile_definitions(bergamot-translator PRIVATE WASM_BINDINGS)
  target_compile_options(bergamot-translator PRIVATE ${WASM_COMPILE_FLAGS})
endif(COMPILE_WASM)

target_link_libraries(bergamot-translator marian ssplit)

target_include_directories(bergamot-translator
    PRIVATE ${CMAKE_SOURCE_DIR}
    PUBLIC ${CMAKE_SOURCE_DIR}/src)


