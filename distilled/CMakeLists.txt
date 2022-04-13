cmake_minimum_required(VERSION 3.5.1)

project(Distilled C CXX)

add_executable(DistilledModel main.cpp birnn.cpp utils.cpp)
target_link_libraries(DistilledModel dl marian ssplit)


