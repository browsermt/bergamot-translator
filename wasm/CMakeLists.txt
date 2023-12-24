add_executable(bergamot-translator-worker
    bindings/service_bindings.cpp
    bindings/response_options_bindings.cpp
    bindings/response_bindings.cpp
)

# Generate version file that can be included in the Wasm artifacts
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/project_version.js.in
               ${CMAKE_CURRENT_BINARY_DIR}/project_version.js @ONLY)

# This header inclusion needs to go away later as path to public headers of bergamot
# translator should be directly available from "bergamot-translator" target
target_include_directories(bergamot-translator-worker
    PRIVATE ${CMAKE_SOURCE_DIR}/src/translator
    PRIVATE ${CMAKE_SOURCE_DIR}
)

# This compile definition is required for generating binding code properly
target_compile_definitions(bergamot-translator-worker PRIVATE WASM_BINDINGS)
target_compile_options(bergamot-translator-worker PRIVATE ${WASM_COMPILE_FLAGS})
target_link_options(bergamot-translator-worker PRIVATE ${WASM_LINK_FLAGS})
target_link_options(bergamot-translator-worker PRIVATE --extern-pre-js=${CMAKE_CURRENT_BINARY_DIR}/project_version.js)

set_target_properties(bergamot-translator-worker PROPERTIES
                        SUFFIX ".js"
                        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
                      )

target_link_libraries(bergamot-translator-worker bergamot-translator)
