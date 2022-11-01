cmake_minimum_required(VERSION 3.13.4)
project(MiniLLI)

set(CMAKE_CXX_STANDARD 17)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Set your project compile flags.
# E.g. if using the C++ header files
# you will need to enable C++11 support
# for your compiler.

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

# Now build our tools
add_executable(mini-lli 
    src/disasm.h
    src/unwind.h
    src/util.h
    src/main.cpp
    )

# Find the libraries that correspond to the LLVM components
# that we wish to use
llvm_map_components_to_libnames(llvm_libs support ORCJIT X86 core irreader)

# Link against LLVM libraries
target_link_libraries(mini-lli ${llvm_libs})