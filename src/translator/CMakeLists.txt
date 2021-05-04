add_library(bergamot-translator STATIC
    byte_array_util.cpp
    text_processor.cpp
    sentence_splitter.cpp
    batch_translator.cpp 
    request.cpp 
    batcher.cpp
    response_builder.cpp
    batch.cpp
    sentence_ranges.cpp
    service.cpp
)
if (USE_WASM_COMPATIBLE_SOURCE)
  # Using wasm compatible sources should include this compile definition;
  # Has to be done here because we are including marian headers + some sources
  # in local repository use these definitions
  target_compile_definitions(bergamot-translator PUBLIC USE_SSE2 WASM_COMPATIBLE_SOURCE)
endif()

if(COMPILE_WASM)
  target_compile_definitions(bergamot-translator PUBLIC WASM)
  # Enable code that is required for generating JS bindings
  target_compile_definitions(bergamot-translator PRIVATE WASM_BINDINGS)
  target_compile_options(bergamot-translator PRIVATE ${WASM_COMPILE_FLAGS})
endif(COMPILE_WASM)

target_link_libraries(bergamot-translator marian ssplit)

target_include_directories(bergamot-translator
    PUBLIC ${PROJECT_SOURCE_DIR}
           ${PROJECT_SOURCE_DIR}/src)
