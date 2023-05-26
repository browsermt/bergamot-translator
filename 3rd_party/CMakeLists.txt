# marian-dev is tested elsewhere in both paths, turning off here.
set(COMPILE_TESTS OFF)
add_subdirectory(marian-dev EXCLUDE_FROM_ALL)

if(COMPILE_WASM)
  # This is a bad way of adding compilation flags. Will be improved soon.
  add_compile_options(${WASM_COMPILE_FLAGS})
  add_link_options(${WASM_LINK_FLAGS})
endif(COMPILE_WASM)

add_subdirectory(ssplit-cpp EXCLUDE_FROM_ALL)

# Add include directories for 3rd party targets to be able to use it anywhere in the
# project without explicitly specifying their include directories. Once they
# fixe this problem, it can be removed.
get_property(INCDIRS DIRECTORY marian-dev/src PROPERTY INCLUDE_DIRECTORIES)
target_include_directories(marian PUBLIC ${INCDIRS})

get_property(INCLUDE_DIRECTORIES DIRECTORY ssplit-cpp/src PROPERTY INCLUDE_DIRECTORIES)
target_include_directories(ssplit PUBLIC ${INCLUDE_DIRECTORIES})

get_property(COMPILE_DEFINITIONS DIRECTORY marian-dev PROPERTY COMPILE_DEFINITIONS) 
target_compile_definitions(marian PUBLIC ${COMPILE_DEFINITIONS})

get_property(COMPILE_OPTIONS DIRECTORY marian-dev PROPERTY COMPILE_OPTIONS) 
target_compile_options(marian PUBLIC ${COMPILE_OPTIONS})

# Compilation flags 
get_directory_property(CMAKE_C_FLAGS DIRECTORY marian-dev DEFINITION CMAKE_C_FLAGS) 
get_directory_property(CMAKE_CXX_FLAGS DIRECTORY marian-dev DEFINITION CMAKE_CXX_FLAGS) 
set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} PARENT_SCOPE)    
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} PARENT_SCOPE)    
