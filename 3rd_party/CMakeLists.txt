add_subdirectory(marian-dev)

# Add include directories for marian target to be able to use it anywhere in the project without
# explicitly specifying its include directories. Once marian fixes this problem, it can be removed.
get_property(INCDIRS DIRECTORY marian-dev/src PROPERTY INCLUDE_DIRECTORIES)
target_include_directories(marian PUBLIC ${INCDIRS})
