add_executable(bergamot-translator-wasm
    bindings/TranslationModelBindings.cpp
    bindings/TranslationModelConfigurationBindings.cpp
    bindings/TranslationRequestBindings.cpp
    bindings/TranslationResultBindings.cpp
)

# This header inclusion needs to go away later as path to public headers of bergamot
# translator should be directly available from "bergamot-translator" target
target_include_directories(bergamot-translator-wasm
    PRIVATE ${CMAKE_SOURCE_DIR}/src/translator
)

set_target_properties(bergamot-translator-wasm PROPERTIES
                        SUFFIX ".js"
                        LINK_FLAGS "--bind --post-js ../../wasm/post-module.js")

target_link_libraries(bergamot-translator-wasm bergamot-translator)
