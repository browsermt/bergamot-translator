add_library(bergamot-translator STATIC
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
if (USE_WASM_COMPATIBLE_MARIAN)
  # Using wasm compatible marian should include this compile definition;
  # Has to be done here because of the marian's cmake practices
  target_compile_definitions(bergamot-translator PUBLIC WASM)
endif()

if(COMPILE_WASM)
  # A dirty hack because of marian's bad cmake practices
  target_compile_definitions(bergamot-translator PUBLIC USE_SSE2)
  # Enable code that is required for generating JS bindings
  target_compile_definitions(bergamot-translator PRIVATE WASM_BINDINGS)
  target_compile_options(bergamot-translator PRIVATE ${WASM_COMPILE_FLAGS})
endif(COMPILE_WASM)

target_link_libraries(bergamot-translator marian ssplit)

target_include_directories(bergamot-translator
    PUBLIC ${CMAKE_SOURCE_DIR}
    PUBLIC ${CMAKE_SOURCE_DIR}/src)
