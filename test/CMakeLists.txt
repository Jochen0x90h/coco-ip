# Generate a test for a board
# TEST the test application, implemented in ${TEST}.cpp
# BOARD_LIB a library for a board containing SystemInit() and a linker script for embedded platforms
function(board_test TEST BOARD_LIB)
    # check if board library exists for the current platform
    if(TARGET ${BOARD_LIB})
        string(REGEX REPLACE ".*\\:" "" BOARD ${BOARD_LIB})
        set(NAME "${TEST}-${BOARD}")
        message("*** Board: ${BOARD}")

        add_executable(${NAME}
            ${TEST}.cpp
        )
        target_include_directories(${NAME}
            PRIVATE
                ../
                ${BOARD}
        )
        target_link_libraries(${NAME}
            ${BOARD_LIB}
            ${PROJECT_NAME}
        )

        # windows specific libraries
        if(WIN32)
            target_link_libraries(${NAME} Ws2_32)
        endif()

        # generate hex file for flashing the target
        if(${CMAKE_CROSSCOMPILING})
            #message("*** Generate Hex for ${NAME} using ${CMAKE_OBJCOPY}")
            add_custom_command(TARGET ${NAME}
                POST_BUILD
                COMMAND ${CMAKE_OBJCOPY} -O ihex ${NAME} ${NAME}.hex
            )
        endif()
    endif()
endfunction()

board_test(Udp4SocketTest coco-devboards::native)
board_test(Udp6SocketTest coco-devboards::native)
board_test(ConnectedUdp6SocketTest coco-devboards::native)



# unit tests on a "normal" operating system such as Windows or Linux
if(NOT ${CMAKE_CROSSCOMPILING})
    add_executable(gTest
        gTest.cpp
    )
    target_include_directories(gTest
        PRIVATE
        ..
    )
    target_link_libraries(gTest
        ${PROJECT_NAME}
        GTest::gtest
    )

    # windows specific libraries
    if(WIN32)
        target_link_libraries(${PROJECT_NAME} Ws2_32)
    endif()

    add_test(NAME gTest
        COMMAND gTest --gtest_output=xml:report.xml
        #WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../testdata
    )
endif()
