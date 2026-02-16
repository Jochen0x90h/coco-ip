#include <coco/convert.hpp>
#include <coco/debug.hpp>
#include "TcpSocketTest.hpp"
#ifdef NATIVE
//#include <string>
//#include <iostream>
#endif


/*
    Tcp4SocketTest: Start a hppt server with $ python -m http.server
    Then start this test.
*/

Coroutine sender(Loop &loop, Buffer &buffer) {
    const uint8_t data[] = {1, 2, 3, 4};
    while (true) {
        co_await buffer.writeArray(data);
#ifndef NATIVE
        debug::toggleRed();
#endif
        debug::out << "Sent " << dec(buffer.size()) << '\n';
        co_await loop.sleep(1s);
    }
}

Coroutine receiver(Loop &loop, Buffer &buffer) {
    while (true) {
        int s = co_await select(buffer.read(), loop.sleep(1s));
        if (s == 1) {
            debug::toggleGreen();
            debug::out << "Received " << dec(buffer.size()) << " from port " << dec(int(buffer.header<ip::Endpoint>().v4.port)) << '\n';
        } else {
            // timeout
            debug::out << "Timeout: Cancel\n";
            co_await buffer.acquire();
        }
    }
}


uint16_t port = 8000;

#ifdef NATIVE
int main(int argc, char const **argv) {
    if (argc >= 2) {
        port = std::stoi(argv[1]);
    }
#else
int main() {
#endif
    debug::out << "Tcp4SocketTest\n";
    debug::out << "Port " << dec(port) << '\n';

    ip::v4::Endpoint endpoint = {.port = port, .address = *ip::v4::Address::fromString("127.0.0.1")};
    drivers.socket.connect(endpoint);

    //sender(drivers.loop, drivers.buffer1);
    receiver(drivers.loop, drivers.buffer2);

    drivers.loop.run();
}
