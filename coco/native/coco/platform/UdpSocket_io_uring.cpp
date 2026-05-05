#include "UdpSocket_io_uring.hpp"
//#include <iostream>


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
        int error = errno;
        setSystemError(errno);
        return false;
    }

    int reuse = 1;
    sockaddr_in6 ep = {.sin6_family = protocolId, .sin6_port = htons(localPort)};

    // reuse address/port
    // https://stackoverflow.com/questions/14388706/how-do-so-reuseaddr-and-so-reuseport-differ
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
        goto error;

    // bind to local port
    if (bind(socket, (struct sockaddr*)&ep, sizeof(ep)) < 0)
        goto error;

    socket_ = socket;
    setSuccess();

    // set state
    state_ = State::READY;

    // enable buffers
    for (auto &buffer : buffers_) {
        buffer.setReady();
    }

    // resume all coroutines waiting for state change
    notify(Events::ENTER_OPENING | Events::ENTER_READY);

    return true;
error:
    int error = errno;
    setSystemError(errno);
    ::close(socket);
    return false;
}

bool UdpSocket_io_uring::join(ip::v6::Address const &multicastGroup) {
    // join multicast group
    struct ipv6_mreq group;
    std::copy(multicastGroup.u8, multicastGroup.u8 + 16, group.ipv6mr_multiaddr.s6_addr);
    group.ipv6mr_interface = 0;
    int r = setsockopt(socket_, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char *)&group, sizeof(group));
    if (r < 0) {
        int error = errno;
        setSystemError(errno);
        return false;
    }
    setSuccess();
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
    setSuccess();

    // set state
    state_ = State::DISABLED;

    // disable buffers
    for (auto &buffer : buffers_) {
        buffer.setDisabled();
    }

    // resume all coroutines waiting for state change
    notify(Events::ENTER_CLOSING | Events::ENTER_DISABLED);
}


// UdpSocket_io_uring::Buffer

UdpSocket_io_uring::Buffer::Buffer(UdpSocket_io_uring &device, int size)
    : coco::Buffer(&endpoint_, sizeof(endpoint_), new uint8_t[size], size, device.state_)
    , device_(device)
{
    device.buffers_.add(*this);
}

UdpSocket_io_uring::Buffer::~Buffer() {
    delete [] data_;
}

bool UdpSocket_io_uring::Buffer::start() {
    if (state_ != State::READY) {
        assert(false);
        setError(std::errc::resource_unavailable_try_again);
        return false;
    }
    if ((op_ & Op::READ_WRITE) == 0 || size_ == 0) {
        setSuccess();
        return false;
    }

    // store read/write flags for use in transfer(), handle() and cancel()
    steps_ = uint8_t(op_ & Op::READ_WRITE);

    // start transfer
    if (!transfer())
        return false;

    // set state
    setBusy();
    return true;
}

bool UdpSocket_io_uring::Buffer::cancel() {
    if (state_ != State::BUSY)
        return false;

    if (steps_ != 0) {
        if (!device_.loop_.cancel(this)) {
            // error: submit buffer full
            setError(std::errc::resource_unavailable_try_again);
            return false;
        }
        steps_ = 0;
    }
    return true;
}

bool UdpSocket_io_uring::Buffer::transfer() {
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

    if (!device_.loop_.transfer((op_ & Op::WRITE) == 0 ? IORING_OP_RECVMSG : IORING_OP_SENDMSG,
        device_.socket_, &message_, 0, this))
    {
        // error: submit queue full
        setError(std::errc::resource_unavailable_try_again);
        setReady();
        return false;
    }
    return true;
}

void UdpSocket_io_uring::Buffer::handle(io_uring_cqe &cqe) {
    int result = cqe.res;
    if (result >= 0) {
        // success
        int transferred = result;
        setSuccess(transferred);
    } else {
        // error
        // EMSGSIZE: datagram was larger than buffer (for UDP)
        // ECANCELED: cancelled
        int error = -result;
        setSystemError(error);
    }

    // transfer finished
    setReady();
}

} // namespace coco
