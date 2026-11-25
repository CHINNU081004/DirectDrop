cmake_minimum_required(VERSION 3.22)

add_library(directdrop_protocol STATIC
    Protocol.hpp
    Protocol.cpp
    PathValidator.hpp
    PathValidator.cpp
    SafeFileWriter.hpp
    SafeFileWriter.cpp
)

target_include_directories(directdrop_protocol PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(directdrop_protocol PUBLIC Qt6::Core)
target_compile_features(directdrop_protocol PUBLIC cxx_std_20)
