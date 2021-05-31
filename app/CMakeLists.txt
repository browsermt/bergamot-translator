add_executable(bergamot bergamot.cpp testapps.cpp)
target_link_libraries(bergamot PRIVATE bergamot-translator)
