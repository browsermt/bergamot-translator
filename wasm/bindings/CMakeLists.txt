add_executable(bergamot-translator-wasm
    TranslationModelBindings.cpp
    TranslationModelConfigurationBindings.cpp
    TranslationRequestBindings.cpp
    TranslationResultBindings.cpp
)

target_include_directories(bergamot-translator-wasm
    PRIVATE ${CMAKE_SOURCE_DIR}/src/translator
    PRIVATE ${CMAKE_SOURCE_DIR}/src
    PRIVATE "/Users/mozilla/work/emsdk/upstream/emscripten/system/include"
)

set_target_properties(bergamot-translator-wasm PROPERTIES SUFFIX ".js")

target_link_libraries(bergamot-translator-wasm bergamot-translator)
set_target_properties(bergamot-translator-wasm PROPERTIES LINK_FLAGS "--bind")
