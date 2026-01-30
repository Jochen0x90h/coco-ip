set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
add_library(${PROJECT_NAME})
target_sources(${PROJECT_NAME}
    PUBLIC FILE_SET headers TYPE HEADERS FILES
        ip.hpp
        IpSocket.hpp
        UdpSocket.hpp
    PRIVATE
        IpSocket.cpp
        UdpSocket.cpp
)

if(${PLATFORM} STREQUAL "native" OR ${PLATFORM} STREQUAL "emu")
    # native platform (Windows, MacOS, Linux)
    target_sources(${PROJECT_NAME}
        PUBLIC FILE_SET platform_headers TYPE HEADERS BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/native FILES
            native/coco/platform/IpSocket_native.hpp
            native/coco/platform/UdpSocket_native.hpp
        PRIVATE
            native/coco/platform/ip.cpp
    )
    if(WIN32)
        # Winsock2
        target_sources(${PROJECT_NAME}
            PUBLIC FILE_SET platform_headers FILES
                native/coco/platform/IpSocket_Win32.hpp
                native/coco/platform/UdpSocket_Win32.hpp
            PRIVATE
                native/coco/platform/IpSocket_Win32.cpp
                native/coco/platform/UdpSocket_Win32.cpp
        )
    endif()
elseif(${PLATFORM} MATCHES "^nrf52")
elseif(${PLATFORM} MATCHES "^stm32f0")

endif()

target_link_libraries(${PROJECT_NAME}
    coco::coco
    coco-loop::coco-loop
    coco-device::coco-device
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ..
)

# install the library
install(TARGETS ${PROJECT_NAME}
    FILE_SET headers DESTINATION include/coco
    FILE_SET platform_headers DESTINATION include
)
