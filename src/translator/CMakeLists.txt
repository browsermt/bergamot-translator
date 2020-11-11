add_library(bergamot-translator STATIC
    AbstractTranslationModel.cpp
    TranslationModel.cpp)

target_link_libraries(bergamot-translator marian)