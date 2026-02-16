#include "UdpSocket_io_uring.hpp"
#include <iostream>


namespace coco {

UdpSocket_io_uring::UdpSocket_io_uring(Loop_io_uring &loop)
    : UdpSocket(State::DISABLED)
    , loop_(loop)
{
}

UdpSocket_io_uring::~UdpSocket_io_uring() {
   ::close(socket_);
}

bool UdpSocket_io_uring::open(uint16_t protocolId, int localPort) {
    if (socket_ != INVALID_SOCKET)
        return false;

    // create socket
    int socket = ::socket(protocolId, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP);
    if (socket == INVALID_SOCKET) {
        int e = errno;
        return false;
    }

    // reuse address/port
    // https://stackoverflow.com/questions/14388706/how-do-so-reuseaddr-and-so-reuseport-differ
    int reuse = 1;
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
        int e = errno;
        ::close(socket);
        return false;
    }

    // bind to local port
    sockaddr_in6 ep = {.sin6_family = protocolId, .sin6_port = htons(localPort)};
    if (bind(socket, (struct sockaddr*)&ep, sizeof(ep)) < 0) {
        int e = errno;
        ::close(socket);
        return false;
    }

    socket_ = socket;

    // set state
    st.set(State::READY);

    // enable buffers
    for (auto &buffer : buffers_) {
        buffer.setReady(0);
    }

    // resume all coroutines waiting for state change
    st.notify(Events::ENTER_OPENING | Events::ENTER_READY);

    return true;
}

bool UdpSocket_io_uring::join(ip::v6::Address const &multicastGroup) {
    // join multicast group
    struct ipv6_mreq group;
    std::copy(multicastGroup.u8, multicastGroup.u8 + 16, group.ipv6mr_multiaddr.s6_addr);
    group.ipv6mr_interface = 0;
    int r = setsockopt(socket_, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&group, sizeof(group));
    if (r < 0) {
        int e = errno;
        return false;
    }
    return true;
}

int UdpSocket_io_uring::getBufferCount() {
    return buffers_.count();
}

UdpSocket_io_uring::Buffer &UdpSocket_io_uring::getBuffer(int index) {
    return buffers_.get(index);
}

void UdpSocket_io_uring::close() {
    if (socket_ == INVALID_SOCKET)
        return;

    // close socket
    ::close(socket_);
    socket_ = INVALID_SOCKET;

    // set state
    st.set(State::DISABLED);

    // disable buffers
    for (auto &buffer : buffers_) {
        buffer.setDisabled();
    }

    // resume all coroutines waiting for state change
    st.notify(Events::ENTER_CLOSING | Events::ENTER_DISABLED);
}


// UdpSocket_io_uring::Buffer

UdpSocket_io_uring::Buffer::Buffer(UdpSocket_io_uring &device, int size)
    : coco::Buffer(&endpoint_, sizeof(endpoint_), 0, new uint8_t[size], size, device.st.state)
    , device_(device)
{
    device.buffers_.add(*this);
}

UdpSocket_io_uring::Buffer::~Buffer() {
    delete [] data_;
}

bool UdpSocket_io_uring::Buffer::start(Op op) {
    if (st.state != State::READY || (op & Op::READ_WRITE) == 0 || size_ == 0) {
        assert(st.state != State::BUSY);
        return false;
    }

    // check if READ or WRITE flag is set
    assert((op & Op::READ_WRITE) != 0);
    op_ = op;

    // add to list of pending transfers
    device_.transfers_.add(*this);

    // start if device is ready
    if (device_.st.state == Device::State::READY)
        start();

    // set state
    setBusy();

    return true;
}

bool UdpSocket_io_uring::Buffer::cancel() {
    if (st.state != State::BUSY)
        return false;

    if ((op_ & Op::CANCEL) == 0) {
        device_.loop_.cancel(this);
        op_ |= Op::CANCEL;
    }

    return true;
}

void UdpSocket_io_uring::Buffer::start() {
    buffer_ = {data_, size_};
    message_ = {
        .msg_name       = &endpoint_,
        .msg_namelen    = sizeof(endpoint_),
        .msg_iov        = &buffer_,
        .msg_iovlen     = 1,
        .msg_control    = nullptr,
        .msg_controllen = 0,
        .msg_flags      = 0  
    };

    device_.loop_.submit((op_ & Op::WRITE) == 0 ? IORING_OP_RECVMSG : IORING_OP_SENDMSG,
        device_.socket_, &message_, 0, this);
}

void UdpSocket_io_uring::Buffer::handle(io_uring_cqe &cqe) {
    int transferred;
    int result = cqe.res;
    if (result >= 0) {
        transferred = result;
    } else {
        // error
        int error = -result;

        // EMSGSIZE: datagram was larger than buffer (for UDP)
        result_ = Result::FAIL;

        transferred = 0;
    }

    // remove from list of active transfers
    remove2();

    // transfer finished
    setReady(transferred);
}

} // namespace coco
