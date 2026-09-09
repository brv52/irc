#include "SocketGuard.hpp"
#include <unistd.h>

SocketGuard::SocketGuard() : _fd(-1) {}

SocketGuard::SocketGuard(int fd) : _fd(fd) {}

SocketGuard::~SocketGuard() {
    if (_fd != -1)
        close(_fd);
}

int SocketGuard::get() const {
    return _fd;
}

int SocketGuard::release() {
    int tmp = _fd;
    _fd = -1;
    return(tmp);
}

void SocketGuard::reset(int newFd) {
    if (_fd != -1)
        close(_fd);
    _fd = newFd;
}
