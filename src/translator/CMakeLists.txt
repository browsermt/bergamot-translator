# Generate version file
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/project_version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/project_version.h @ONLY)

add_library(bergamot-translator STATIC
    byte_array_util.cpp
    text_processor.cpp
    translation_model.cpp 
    request.cpp 
    batching_pool.cpp
    aggregate_batching_pool.cpp
    response_builder.cpp
    quality_estimator.cpp
    batch.cpp
    annotation.cpp
    service.cpp
    terminology.cpp
    parser.cpp
    response.cpp
    html.cpp
    xh_scanner.cpp
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
  target_link_options(bergamot-translator PRIVATE ${WASM_LINK_FLAGS})
endif(COMPILE_WASM)

if(ENABLE_CACHE_STATS)
    target_compile_definitions(bergamot-translator PUBLIC ENABLE_CACHE_STATS)
endif(ENABLE_CACHE_STATS)

if(COMPILE_CUDA)
  target_link_libraries(bergamot-translator PUBLIC marian_cuda marian ssplit)
else(COMPILE_CUDA)
  target_link_libraries(bergamot-translator marian ssplit)
endif(COMPILE_CUDA)

target_include_directories(bergamot-translator
    PUBLIC ${PROJECT_SOURCE_DIR}
           ${PROJECT_SOURCE_DIR}/src)
