add_subdirectory(marian-dev)
add_subdirectory(ssplit-cpp)

include_directories(ssplit-cpp/src)

# Add include directories for marian target to be able to use it anywhere in the
# project without explicitly specifying its include directories. Once marian
# fixes this problem, it can be removed.

get_property(INCDIRS DIRECTORY marian-dev/src PROPERTY INCLUDE_DIRECTORIES)
target_include_directories(marian PUBLIC ${INCDIRS})


get_property(INCLUDE_DIRECTORIES DIRECTORY . PROPERTY INCLUDE_DIRECTORIES)
set(INCLUDE_DIRECTORIES ${INCLUDE_DIRECTORIES} PARENT_SCOPE)

# Required to enable MKL, at least
get_directory_property(EXT_LIBS DIRECTORY marian-dev DEFINITION EXT_LIBS)
set(EXT_LIBS ${EXT_LIBS} PARENT_SCOPE)

# Compilation flags
get_directory_property(CMAKE_C_FLAGS DIRECTORY marian-dev DEFINITION CMAKE_C_FLAGS)
get_directory_property(CMAKE_CXX_FLAGS DIRECTORY marian-dev DEFINITION CMAKE_CXX_FLAGS)
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} PARENT_SCOPE)
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} PARENT_SCOPE)

