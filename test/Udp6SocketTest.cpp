#include <coco/convert.hpp>
#include <coco/debug.hpp>
#include "UdpSocketTest.hpp"
#ifdef NATIVE
#include <string>
#include <iostream>
#endif


/*
    UdpSocketTest: Either start one instance without arguments that sends to itself on port 1337
    or start two instances, one with arguments 1337 1338 and one with arguments 1338 1337.
    The sender toggles the red LED, the receiver toggles the green LED.
*/

Coroutine sender(Loop &loop, Buffer &buffer) {
    const uint8_t data[] = {1, 2, 3, 4};
    while (true) {
        co_await buffer.writeArray(data);
        debug::toggleRed();
        debug::out << "Sent " << dec(buffer.size()) << " to port " << dec(int(buffer.header<ip::Endpoint>().v6.port)) << '\n';
        co_await loop.sleep(1s);
    }
}

Coroutine receiver(Loop &loop, Buffer &buffer) {
    while (true) {
        co_await buffer.read();
        debug::toggleGreen();
        debug::out << "Received " << dec(buffer.size()) << " from port " << dec(int(buffer.header<ip::Endpoint>().v6.port)) << '\n';
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
    debug::out << "Udp6SocketTest\n";
    debug::out << "Local port " << dec(localPort) << '\n';
    debug::out << "Remote port " << dec(remotePort) << '\n';

    drivers.socket.open(ip::v6::PROTOCOL_ID, localPort);

    drivers.buffer1.header<ip::v6::Endpoint>() = {.port = remotePort, .address = *ip::v6::Address::fromString("::1")};
    sender(drivers.loop, drivers.buffer1);

    receiver(drivers.loop, drivers.buffer2);

    drivers.loop.run();
}
