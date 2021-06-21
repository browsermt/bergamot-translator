add_executable(bergamot-translator-worker
    bindings/service_bindings.cpp
    bindings/response_options_bindings.cpp
    bindings/response_bindings.cpp
)

# Generate version file that can be included in the wasm artifacts
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

set(LINKER_FLAGS "-g2 --bind -s ASSERTIONS=0 -s DISABLE_EXCEPTION_CATCHING=1 -s ALLOW_MEMORY_GROWTH=1 -s NO_DYNAMIC_EXECUTION=1 -s EXPORTED_RUNTIME_METHODS=[addOnPreMain]")

# Avoid node.js-code in emscripten glue-code
set(LINKER_FLAGS "${LINKER_FLAGS} -s ENVIRONMENT=web,worker")

# Append version information in the Javascript artifact
set(LINKER_FLAGS "${LINKER_FLAGS} --extern-pre-js ${CMAKE_CURRENT_BINARY_DIR}/project_version.js")

set_target_properties(bergamot-translator-worker PROPERTIES
                        SUFFIX ".js"
                        LINK_FLAGS ${LINKER_FLAGS}
                        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
                      )

target_link_libraries(bergamot-translator-worker bergamot-translator)
