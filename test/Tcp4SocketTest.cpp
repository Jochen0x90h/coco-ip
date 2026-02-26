#include <coco/convert.hpp>
#include <coco/debug.hpp>
#include "TcpSocketTest.hpp"


/*
    Tcp4SocketTest: Start a hppt server with $ python -m http.server
    Then start this test.
*/

Coroutine request(Loop &loop, IpSocket &socket, Buffer &buffer) {
    // wait until connected
    co_await socket.untilReadyOrDisabled();
    if (!socket.ready()) {
        // error: probably no server running on the given port
        debug::out << "Error: " << socket.error().message() << '\n';
        loop.exit();
        co_return;
    }

    // sent request
    co_await buffer.write("GET / HTTP/1.1\r\nHost: localhost:8000\r\nConnection: close\r\n\r\n");

    while (true) {
        // wait for reply
        int s = co_await select(buffer.read(), loop.sleep(2s));
        if (s == 1) {
            debug::toggleGreen();
            if (socket.ready()) {
                debug::out << buffer.string() << '\n';
            } else {
                debug::out << "Socket closed by peer\n";
                break;
            }
        } else {
            // timeout
            debug::out << "Timeout: Cancel and close\n";
            co_await buffer.acquire();
            socket.close();
            break;
        }
    }
    loop.exit();
}


uint16_t port = 8000;

#ifdef NATIVE
int main(int argc, char const **argv) {
    if (argc >= 2) {
        port = *dec<int>(argv[1]);
    }
#else
int main() {
#endif
    debug::out << "Tcp4SocketTest\n";
    debug::out << "Port " << dec(port) << '\n';

    ip::v4::Endpoint endpoint = {.port = port, .address = *ip::v4::Address::fromString("127.0.0.1")};
    if (!drivers.socket.connect(endpoint)) {
        debug::out << "Connect failed\n";
        return 1;
    }

    request(drivers.loop, drivers.socket, drivers.buffer);

    drivers.loop.run();
}
