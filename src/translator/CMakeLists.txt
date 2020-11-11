add_library(bergamot-translator STATIC
    AbstractTranslationModel.cpp
    TranslationModel.cpp)

target_link_libraries(bergamot-translator marian)

target_include_directories(bergamot-translator
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PUBLIC ${CMAKE_SOURCE_DIR}/src)
