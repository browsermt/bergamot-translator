add_subdirectory(marian-dev)
add_subdirectory(ssplit-cpp)

# Add include directories for 3rd party targets to be able to use it anywhere in the
# project without explicitly specifying their include directories. Once they
# fixe this problem, it can be removed.
get_property(INCDIRS DIRECTORY marian-dev/src PROPERTY INCLUDE_DIRECTORIES)
target_include_directories(marian PUBLIC ${INCDIRS})

get_property(INCLUDE_DIRECTORIES DIRECTORY ssplit-cpp/src PROPERTY INCLUDE_DIRECTORIES)
target_include_directories(ssplit PUBLIC ${INCLUDE_DIRECTORIES})
