#include "User.hpp"
#include <unistd.h>
#include <sstream>

User::User() 
:   _socket(-1), _rBuffer(), _wBuffer(), 
    _nickname(), _username(),
    _loggedIn(false), _registered(false) {}

User::User(const User& src) {
    *this = src;
}

User& User::operator=(const User& rhs) {
    if (this != &rhs) {
        _socket = rhs._socket;
        _rBuffer = rhs._rBuffer;
        _wBuffer = rhs._wBuffer;
        _nickname = rhs._nickname;
        _username = rhs._username;
        _loggedIn = rhs._loggedIn;
        _registered = rhs._registered;
    }
    return *this;
}

User::User(int fd)
:   _socket(fd), _rBuffer(), _wBuffer(), 
    _nickname(), _username(),
    _loggedIn(false), _registered(false) {}

User::~User() {}

int User::socket() {
    return _socket;
}

std::string& User::readBuffer() {
    return _rBuffer;
}

std::string& User::writeBuffer() {
    return _wBuffer;
}

void User::logIn() {
    _loggedIn = true;
}

bool User::loggedIn() {
    return _loggedIn;
}

std::string& User::nickname() {
    return _nickname;
}

std::string& User::username() {
    return _username;
}

bool User::registered() {
    return _registered;
}

void User::reg() {
    _registered = true;
}
