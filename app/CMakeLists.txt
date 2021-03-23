add_executable(bergamot-translator-app bergamot-translator-app.cpp)
target_link_libraries(bergamot-translator-app PRIVATE bergamot-translator)

add_executable(bergamot-translator-app-bytearray bergamot-translator-app-bytearray.cpp)
target_link_libraries(bergamot-translator-app-bytearray PRIVATE bergamot-translator)

if (NOT USE_WASM_COMPATIBLE_SOURCES)
    add_executable(service-cli service-cli.cpp)
    target_link_libraries(service-cli PRIVATE bergamot-translator)

    add_executable(service-cli-bytearray service-cli-bytearray.cpp)
    target_link_libraries(service-cli-bytearray PRIVATE bergamot-translator)

    add_executable(marian-decoder-new marian-decoder-new.cpp)
    target_link_libraries(marian-decoder-new PRIVATE bergamot-translator)

    add_executable(concurrent-test-app concurrent-test-app.cpp)
    target_link_libraries(concurrent-test-app PRIVATE bergamot-translator)
endif()
