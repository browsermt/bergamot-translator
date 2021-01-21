add_library(bergamot-translator STATIC
    AbstractTranslationModel.cpp
    TranslationModel.cpp
    TranslationModelConfigToOptionsAdaptor.cpp

    # Following files added from browsermt/mts@nuke
    textops.cpp
    batch_translator.cpp 
    multifactor_priority.cpp 
    request.cpp 
    service.cpp
    batcher.cpp
    translation_result.cpp
)

target_link_libraries(bergamot-translator marian)
target_include_directories(bergamot-translator
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}
    PUBLIC ${CMAKE_SOURCE_DIR}/src)

