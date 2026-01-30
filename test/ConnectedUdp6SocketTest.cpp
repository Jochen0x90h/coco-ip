#include <coco/convert.hpp>
#include <coco/debug.hpp>
#include "ConnectedUdpSocketTest.hpp"
#ifdef NATIVE
#include <string>
#include <iostream>
#endif


/*
    ConnectedUdpSocketTest: Either start one instance without arguments that sends to itself on port 1337
    or start two instances, one with arguments 1337 1338 and one with arguments 1338 1337.
    The sender toggles the red LED, the receiver toggles the green LED.
*/

Coroutine sender(Loop &loop, Buffer &buffer) {
    const uint8_t data[] = {1, 2, 3, 4};
    while (true) {
        co_await buffer.writeArray(data);
        debug::toggleRed();
        debug::out << "Sent " << dec(buffer.size()) << '\n';
        co_await loop.sleep(1s);
    }
}

Coroutine receiver(Loop &loop, Buffer &buffer) {
    while (true) {
        co_await buffer.read();
        debug::toggleGreen();
        debug::out << "Received " << dec(buffer.size()) << '\n';
    }
}

// it is possible to start two instances with different ports by passing localPort and remotePort as arguments
uint16_t localPort = 1337;
uint16_t remotePort = 1337;

#ifdef NATIVE
int main(int argc, char const **argv) {
    if (argc >= 3) {
        localPort = std::stoi(argv[1]);
        remotePort = std::stoi(argv[2]);
    }
#else
int main() {
#endif
    debug::out << "ConnectedUdp6SocketTest\n";
    debug::out << "Local port " << dec(localPort) << '\n';
    debug::out << "Remote port " << dec(remotePort) << '\n';

    ip::v6::Endpoint endpoint = {.port = remotePort, .address = *ip::v6::Address::fromString("::1")};
    drivers.socket.connect(endpoint, localPort);

    sender(drivers.loop, drivers.buffer1);

    receiver(drivers.loop, drivers.buffer2);

    drivers.loop.run();
}
