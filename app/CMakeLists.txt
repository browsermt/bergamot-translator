add_executable(bergamot-translator-app bergamot-translator-app.cpp)
target_link_libraries(bergamot-translator-app PRIVATE bergamot-translator)

if (NOT USE_WASM_COMPATIBLE_SOURCE)
    add_executable(service-cli service-cli.cpp)
    target_link_libraries(service-cli PRIVATE bergamot-translator)
    target_link_libraries(service-cli PRIVATE onnx_proto)
    target_link_libraries(service-cli PRIVATE protobuf)

    add_executable(marian-decoder-new marian-decoder-new.cpp)
    target_link_libraries(marian-decoder-new PRIVATE bergamot-translator)
endif()
