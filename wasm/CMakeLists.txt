add_executable(bergamot-translator-worker
    bindings/TranslationModelBindings.cpp
    bindings/TranslationRequestBindings.cpp
    bindings/TranslationResultBindings.cpp
)

# This header inclusion needs to go away later as path to public headers of bergamot
# translator should be directly available from "bergamot-translator" target
target_include_directories(bergamot-translator-worker
    PRIVATE ${CMAKE_SOURCE_DIR}/src/translator
    PRIVATE ${CMAKE_SOURCE_DIR}
)
# This compile definition is required for generating binding code properly
target_compile_definitions(bergamot-translator-worker PRIVATE WASM_BINDINGS)
target_compile_options(bergamot-translator-worker PRIVATE ${WASM_COMPILE_FLAGS})

set(LINKER_FLAGS "--bind -s ASSERTIONS=0 -s DISABLE_EXCEPTION_CATCHING=1 -s FORCE_FILESYSTEM=1 -s ALLOW_MEMORY_GROWTH=1 -s NO_DYNAMIC_EXECUTION=1")
if (NOT PACKAGE_DIR STREQUAL "")
  get_filename_component(REALPATH_PACKAGE_DIR ${PACKAGE_DIR} REALPATH BASE_DIR ${CMAKE_BINARY_DIR})
  set(LINKER_FLAGS "${LINKER_FLAGS} --preload-file ${REALPATH_PACKAGE_DIR}@/")
endif()

# Enable worker file system
set(LINKER_FLAGS "${LINKER_FLAGS} -lworkerfs.js")

# Avoid node.js-code in emscripten glue-code
set(LINKER_FLAGS "${LINKER_FLAGS} -s ENVIRONMENT=web,worker")

set_target_properties(bergamot-translator-worker PROPERTIES
                        SUFFIX ".js"
                        LINK_FLAGS ${LINKER_FLAGS}
                        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
                      )

target_link_libraries(bergamot-translator-worker bergamot-translator)
