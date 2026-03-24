find_package(OpenSSL REQUIRED)

add_library(smouse_net
    tcp_connection.cpp
    udp_channel.cpp
    tls_context.cpp
    discovery.cpp
    ble_proximity.cpp
)

target_include_directories(smouse_net PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(smouse_net PUBLIC smouse_core OpenSSL::SSL OpenSSL::Crypto)

# Platform-specific socket libraries
if(WIN32)
    target_link_libraries(smouse_net PRIVATE ws2_32)
endif()
