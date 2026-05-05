#include "IpSocket_io_uring.hpp"


namespace coco {

IpSocket_io_uring::IpSocket_io_uring(Loop_io_uring &loop, int type, int protocol)
    : IpSocket(State::DISABLED)
    , loop_(loop)
    , type_(type), protocol_(protocol)
{
}

IpSocket_io_uring::~IpSocket_io_uring() {
    ::close(socket_);
}

int IpSocket_io_uring::getBufferCount() {
    return buffers_.count();
}

IpSocket_io_uring::Buffer &IpSocket_io_uring::getBuffer(int index) {
    return buffers_.get(index);
}

bool IpSocket_io_uring::connect(const ip::Endpoint &endpoint, int size, int localPort) {
    if (socket_ != INVALID_SOCKET)
        return false;

    // create socket
    int socket = ::socket(endpoint.protocolId, type_ | SOCK_NONBLOCK, protocol_);
    if (socket == INVALID_SOCKET) {
        int error = errno;
        setSystemError(errno);
        return false;
    }

    int reuse = 1;
    sockaddr_in6 local = {.sin6_family = endpoint.protocolId, .sin6_port = htons(localPort)};

    // reuse address/port
    // https://stackoverflow.com/questions/14388706/how-do-so-reuseaddr-and-so-reuseport-differ
    if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0)
        goto error;

    // bind to any local address/port (required by ConnectEx)
    if (bind(socket, (sockaddr *)&local, sizeof(local)) < 0)
        goto error;

    Events events;
    if (type_ == SOCK_DGRAM) {
        // connect UDP
        if (::connect(socket, (struct sockaddr *)&endpoint, size) < 0)
            goto error;
        socket_ = socket;
        setSuccess();

        // set state
        state_ = State::READY;

        // set buffers READY
        for (auto &buffer : buffers_) {
            buffer.setReady();
        }

        events = Events::ENTER_OPENING | Events::ENTER_READY;
    } else {
        // connect TCP
        if (!loop_.connect(socket, endpoint, this)) {
            // error: submit queue full
            setError(std::errc::resource_unavailable_try_again);
            ::close(socket);
            return false;
        }
        socket_ = socket;
        setSuccess();

        // set state
        state_ = State::OPENING;

        events = Events::ENTER_OPENING;
    }

    // resume all coroutines waiting for state change
    notify(events);

    return true;
error:
    int error = errno;
    setSystemError(errno);
    ::close(socket);
    return false;
}

void IpSocket_io_uring::close() {
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

void IpSocket_io_uring::handle(io_uring_cqe &cqe) {
    int result = cqe.res;
    if (result == -EINPROGRESS) {
        // connect is in progress
    } else if (result < 0) {
        // error
        // ECANCELED: cancelled
        // ECONNREFUSED: connection refused
        auto error = -result;
        setSystemError(error);

        // close socket
        ::close(socket_);
        socket_ = INVALID_SOCKET;

        // set state
        state_ = State::DISABLED;

        // resume all coroutines waiting for state change
        notify(Events::ENTER_DISABLED);

    } else if (result & POLLOUT) {
        // connected

        // set state
        state_ = State::READY;

        // set buffers READY
        for (auto &buffer : buffers_) {
            buffer.setReady();
        }

        // resume all coroutines waiting for state change
        notify(Events::ENTER_READY);
    }
}


// IpSocket_io_uring::Buffer

IpSocket_io_uring::Buffer::Buffer(IpSocket_io_uring &device, int size)
    : coco::Buffer(new uint8_t[size], size, device.state_)
    , device_(device)
{
    device.buffers_.add(*this);
}

IpSocket_io_uring::Buffer::~Buffer() {
    delete [] data_;
}

bool IpSocket_io_uring::Buffer::start() {
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

    // add to list of pending transfers
    //device_.transfers_.add(*this);

    // start transfer
    if (!transfer())
        return false;

    // set state
    setBusy();
    return true;
}

bool IpSocket_io_uring::Buffer::cancel() {
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

bool IpSocket_io_uring::Buffer::transfer() {
    if (!device_.loop_.transfer((Op(steps_) & Op::WRITE) == 0 ? IORING_OP_RECV : IORING_OP_SEND,
        device_.socket_, data_, size_, this))
    {
        // error: submit queue full
        setError(std::errc::resource_unavailable_try_again);
        setReady();
        return false;
    }
    return true;
}

void IpSocket_io_uring::Buffer::handle(io_uring_cqe &cqe) {
    int result = cqe.res;
    if (result >= 0) {
        // success
        int transferred = result;

        // check for connection closed by peer
        if (Op(steps_) == Op::READ && transferred == 0) {
            device_.close();
            return;
        }

        // check for read after write
        if (Op(steps_) == Op::READ_WRITE) {
            steps_ = int(Op::READ);
            transfer();
            return;
        }

        setSuccess(transferred);
    } else {
        // error
        // ECANCELED: cancelled
        // EMSGSIZE: datagram was larger than buffer (for UDP)
        int error = -result;
        setSystemError(error);
    }

    // transfer finished
    setReady();
}

} // namespace coco
