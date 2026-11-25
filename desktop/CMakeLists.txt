cmake_minimum_required(VERSION 3.22)
project(DirectDropDesktop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets DBus Test)

add_subdirectory(../protocol protocol_build)

add_executable(directdrop_p2p_diag
    p2p_diag.cpp
    WifiP2pBackend.hpp
    WifiP2pBackend.cpp
)
target_link_libraries(directdrop_p2p_diag PRIVATE directdrop_protocol Qt6::Core Qt6::DBus)

add_executable(directdrop_desktop
    main.cpp
    MainWindow.hpp
    MainWindow.cpp
    WifiP2pBackend.hpp
    WifiP2pBackend.cpp
    TlsServer.hpp
    TlsServer.cpp
)
target_link_libraries(directdrop_desktop PRIVATE directdrop_protocol Qt6::Core Qt6::Network Qt6::Widgets Qt6::DBus)

enable_testing()
add_subdirectory(../tests tests_build)
