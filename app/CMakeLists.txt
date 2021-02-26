add_executable(bergamot-translator-app main.cpp)
target_link_libraries(bergamot-translator-app PRIVATE bergamot-translator)

add_executable(service-cli main-mts.cpp)
target_link_libraries(service-cli PRIVATE bergamot-translator)

add_executable(marian-decoder-new marian-decoder-new.cpp)
target_link_libraries(marian-decoder-new PRIVATE bergamot-translator)
