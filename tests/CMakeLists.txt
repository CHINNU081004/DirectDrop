cmake_minimum_required(VERSION 3.22)

add_executable(directdrop_tests
    test_protocol_and_files.cpp
    ../desktop/TlsServer.hpp
    ../desktop/TlsServer.cpp
)
target_include_directories(directdrop_tests PRIVATE ../desktop ../protocol)
target_link_libraries(directdrop_tests PRIVATE directdrop_protocol Qt6::Core Qt6::Network Qt6::Test)

add_test(NAME DirectDropTests COMMAND directdrop_tests)
