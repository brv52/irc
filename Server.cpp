#include "Server.hpp"
#include "SocketGuard.hpp"
#include <sys/socket.h>
#include <stdexcept>
#include <cstring>
#include <cerrno>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include "algorithm"
#include <arpa/inet.h>
#include <sstream>
#include <iostream>

Server::Server(int port, const std::string& password)
:   _socket(-1), _port(port), _password(password), _connections()
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        throw std::runtime_error("[socket()] => " + std::string(strerror(errno)));
    SocketGuard sg(fd);

    int opt = 1;
    if (setsockopt(sg.get(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error("[setsockopt()] => " + std::string(strerror(errno)));

    if (fcntl(sg.get(), F_SETFL, O_NONBLOCK) < 0)
        throw std::runtime_error("[fcntl()] => " + std::string(strerror(errno)));

    sockaddr_in address;
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(sg.get(), (sockaddr *)&address, sizeof(address)) < 0)
        throw std::runtime_error("[bind()] => " + std::string(strerror(errno)));

    if (listen(sg.get(), SOMAXCONN) < 0)
        throw std::runtime_error("[listen()] => " + std::string(strerror(errno)));
    
    _socket = sg.release();

    pollfd serverPoll = {_socket, POLLIN, 0};
    _connections.push_back(serverPoll);
}

Server::~Server() {
    if (_socket != -1)
        close(_socket);
}

void Server::run() {
    std::cout << "[Server] Started and listening on port " << _port << "..." << std::endl;

    while(true) {
        for(std::size_t i = 1; i < _connections.size(); ++i) {
            int userSocket = _connections[i].fd;
            if (!_users[userSocket].writeBuffer().empty())
                _connections[i].events |= POLLOUT;
            else
                _connections[i].events &= ~POLLOUT;
        }

        int pollEvents = poll(&_connections[0], _connections.size(), -1); 
        if (pollEvents == -1) {
            if (errno == EINTR)
                continue ;
            std::cerr << "[Server Error] poll(): " << strerror(errno) << std::endl;
            continue ;
        }
        
        for(std::size_t i = 0; i < _connections.size();) {
            if (_connections[i].fd == _socket) {
                if (_connections[i].revents & POLLIN)
                    acceptUser();
                ++i;
                continue ;
            }
            int userSocket = _connections[i].fd;
            bool alive = true;

            if (_connections[i].revents & POLLIN)
                alive = readData(userSocket);
            
            if (alive && (_connections[i].revents & POLLOUT))
                alive = sendData(userSocket);
            
            if (!alive) {
                std::cout << "[Server] Client " << userSocket << " disconnected." << std::endl;
                
                std::map<std::string, Channel>::iterator it = _channels.begin();
                while (it != _channels.end()) {
                    it->second.removeMember(userSocket);

                    if (it->second.isEmpty()) {
                        std::map<std::string, Channel>::iterator deleteIt = it;
                        ++it;
                        _channels.erase(deleteIt);
                    } else {
                        std::string quitMsg = ":" + _users[userSocket].nickname() + " QUIT :Client disconnected\r\n";
                        it->second.broadcast(quitMsg);
                        ++it;
                    }
                }

                _users.erase(userSocket);
                _connections.erase(_connections.begin() + i);
                close(userSocket);
            } else {
                ++i;
            }
        }
    }
}

void Server::acceptUser() {
    sockaddr_in userAddr;
    socklen_t userLen = sizeof(userAddr);
    int userSocket = accept(_socket, (sockaddr *)&userAddr, &userLen);

    if(userSocket == -1) {
        std::cerr << "[Server Error] Failed to accept connection: " << strerror(errno) << std::endl;
        return ;
    }
    if (fcntl(userSocket, F_SETFL, O_NONBLOCK) == -1) {
        std::cerr << "[Server Error] Failed to set non-blocking mode for fd " << userSocket << std::endl;
        close(userSocket);
        return ;
    }

    std::cout << "[Server] New connection from " << inet_ntoa(userAddr.sin_addr) 
              << ":" << ntohs(userAddr.sin_port) << " (fd: " << userSocket << ")" << std::endl;

    pollfd userPoll = {userSocket, POLLIN, 0};
    _connections.push_back(userPoll);
    _users[userSocket] = User(userSocket);
}

bool Server::readData(int socket) {
    char buffer[1024];
    ssize_t bytes = recv(socket, buffer, sizeof(buffer) - 1, 0);

    if (bytes <= 0)
        return false;

    buffer[bytes] = '\0';
    _users[socket].readBuffer() += buffer;

    size_t delimPos;
    while ((delimPos = _users[socket].readBuffer().find("\r\n")) != std::string::npos) {
        std::string cmd = _users[socket].readBuffer().substr(0, delimPos);
        _users[socket].readBuffer().erase(0, delimPos + 2);

        std::cout << "[Client " << socket << "] " << cmd << std::endl;
        processCommand(_users[socket], cmd);
    }

    return true;
}

bool Server::sendData(int socket) {
    std::string& buffer = _users[socket].writeBuffer();

    if (buffer.empty()) {
        return true;
    }

    ssize_t bytes = send(socket, buffer.c_str(), buffer.length(), 0);
    if (bytes <= 0)
        return false;
    
    std::string logStr = buffer.substr(0, bytes);
    if (logStr.length() >= 2 && logStr.substr(logStr.length() - 2) == "\r\n")
        std::cout << "[Server -> Client " << socket << "] " << logStr.substr(0, logStr.length() - 2) << std::endl;
    else
        std::cout << "[Server -> Client " << socket << "] " << logStr << std::endl;

    buffer.erase(0, bytes);

    return true;
}

void Server::processCommand(User& user, const std::string& cmd) {
    size_t delimPos = cmd.find(' ');
    std::string cName = cmd.substr(0, delimPos);
    std::string args = (delimPos != std::string::npos) ? cmd.substr(delimPos + 1) : "";

    std::string clientName = user.nickname().empty() ? "*" : user.nickname();

    if (cName == "PASS") {
        if (user.registered()) {
            user.writeBuffer() += ":ircserver 462 " + clientName + " :You may not reregister\r\n";
            return;
        }
        if (args == _password) {
            user.logIn();
            std::cout << "[Server] Client " << user.socket() << " provided correct password." << std::endl;
        } else {
            std::cout << "[Server] Client " << user.socket() << " provided INCORRECT password." << std::endl;
            user.writeBuffer() += ":ircserver 464 " + clientName + " :Password incorrect\r\n";
        }
    } else if (cName == "NICK") {
        if (!user.loggedIn()) {
            user.writeBuffer() += ":ircserver 451 " + clientName + " :You have not registered (Enter PASS first)\r\n";
            return;
        }
        if (args.empty()) {
            user.writeBuffer() += ":ircserver 431 " + clientName + " :No nickname given\r\n";
            return;
        }
        user.nickname() = args;
        std::cout << "[Server] Client " << user.socket() << " set nickname to '" << args << "'." << std::endl;
    } else if (cName == "USER") {
        if (!user.loggedIn()) {
            user.writeBuffer() += ":ircserver 451 " + clientName + " :You have not registered (Enter PASS first)\r\n";
            return;
        }
        if (user.registered()) {
            user.writeBuffer() += ":ircserver 462 " + clientName + " :You may not reregister\r\n";
            return;
        }
        size_t spacePos = args.find(' ');
        user.username() = (spacePos != std::string::npos) ? args.substr(0, spacePos) : args;
        std::cout << "[Server] Client " << user.socket() << " set username to '" << user.username() << "'." << std::endl;
    } else if (cName == "JOIN") {
        if (!user.registered()) {
            user.writeBuffer() += ":ircserver 451 " + clientName + " :You have not registered\r\n";
            return;
        }
        if (!args.empty() && (args[args.length() - 1] == '\r'))
            args.erase(args.length() - 1);
        
        handleJoin(user, args);
    } else if (cName == "PRIVMSG") {
        if (!user.registered()) {
            user.writeBuffer() += ":ircserver 451 " + clientName + " :You have not registered\r\n";
            return ;
        }
        if (!args.empty() && (args[args.length() - 1] == '\r'))
            args.erase(args.length() - 1);
        
        handleMsg(user, args);
    } else if (cName == "KICK") {
        if (!user.registered()) { 
            user.writeBuffer() += ":ircserver 451 * :You have not registered\r\n"; return;
        }
        if (!args.empty() && args[args.length() - 1] == '\r') 
            args.erase(args.length() - 1);
        handleKick(user, args);
    } else if (cName == "INVITE") {
        if (!user.registered()) {
            user.writeBuffer() += ":ircserver 451 * :You have not registered\r\n"; return;
        }
        if (!args.empty() && args[args.length() - 1] == '\r')
            args.erase(args.length() - 1);
        handleInvite(user, args);
    } else if (cName == "TOPIC") {
        if (!user.registered()) {
            user.writeBuffer() += ":ircserver 451 * :You have not registered\r\n"; return;
        }
        if (!args.empty() && args[args.length() - 1] == '\r')
            args.erase(args.length() - 1);
        handleTopic(user, args);
    } else if (cName == "MODE") {
        if (!user.registered()) {
            user.writeBuffer() += ":ircserver 451 * :You have not registered\r\n"; return;
        }
        if (!args.empty() && args[args.length() - 1] == '\r')
            args.erase(args.length() - 1);
        handleMode(user, args);
    } else if (cName == "PING") {
        if (!args.empty() && args[args.length() - 1] == '\r')
            args.erase(args.length() - 1);
        handlePing(user, args);
    } else {
        user.writeBuffer() += ":ircserver 421 " + clientName + " " + cName + " :Unknown command\r\n";
    }

    if (!user.registered() && user.loggedIn() && !user.nickname().empty() && !user.username().empty()) {
        user.reg();
        std::cout << "[Server] Client " << user.socket() << " successfully registered as " << user.nickname() << "!" << std::endl;
        std::string welomeMsg = ":ircserver 001 " + user.nickname() + " :Welcome to the Internet Relay Network " + user.nickname() + "\r\n";
        user.writeBuffer() += welomeMsg;
    }
}

void Server::handleJoin(User& user, const std::string& args) {
    size_t delim = args.find(' ');
    std::string channel = (delim != std::string::npos) ? args.substr(0, delim) : args;
    std::string key = (delim != std::string::npos) ? args.substr(delim + 1) : "";

    if (channel.empty() || channel[0] != '#') {
        std::string target = channel.empty() ? "*" : channel;
        user.writeBuffer() += ":ircserver 403 " + user.nickname() + " " + target + " :No such channel\r\n";
        return;
    }

    bool isNew = (_channels.find(channel) == _channels.end());

    if (!isNew) {
        Channel& c = _channels.find(channel)->second;
        if (c.isInviteOnly() && !c.isInvited(user.nickname())) {
            user.writeBuffer() += ":ircserver 473 " + user.nickname() + " " + channel + " :Cannot join channel (+i)\r\n";
            return;
        }
        if (c.getLimit() > 0 && c.getMembersCount() >= c.getLimit()) {
            user.writeBuffer() += ":ircserver 471 " + user.nickname() + " " + channel + " :Cannot join channel (+l)\r\n";
            return;
        }
        if (!c.getPassword().empty() && c.getPassword() != key) {
            user.writeBuffer() += ":ircserver 475 " + user.nickname() + " " + channel + " :Cannot join channel (+k)\r\n";
            return;
        }
    } else {
        _channels.insert(std::make_pair(channel, Channel(channel)));
    }
    
    Channel& c = _channels.find(channel)->second;
    c.addMember(&user, isNew);

    std::string prefix = ":" + user.nickname() + "!" + user.username() + "@localhost";
    std::string joinBroadcast = prefix + " JOIN :" + channel + "\r\n";
    c.broadcast(joinBroadcast);

    std::string names = c.getNames();
    std::string reply353 = ":ircserver 353 " + user.nickname() + " = " + channel + " :" + names + "\r\n";
    std::string reply366 = ":ircserver 366 " + user.nickname() + " " + channel + " :End of /NAMES list.\r\n";

    user.writeBuffer() += reply353 + reply366;

    std::cout << "[Server] " << user.nickname() << " joined " << channel << std::endl;
    if (!isNew)
        c.removeInvite(user.nickname());
}

std::map<int, User>::iterator Server::findUserNickname(const std::string& target) {
    std::map<int, User>::iterator res;
    for (res = _users.begin(); res != _users.end(); ++res) {
        if (res->second.nickname() == target)
            break;
    }
    return res;
}

void Server::handleMsg(User& user, const std::string& args) {
    size_t delimPos = args.find(' ');
    if (delimPos == std::string::npos || args.substr(delimPos + 1).empty()) {
        user.writeBuffer() += ":ircserver 412 " + user.nickname() + " :No text to send\r\n";
        return;
    }
    
    std::string target = args.substr(0, delimPos);
    std::string msg = args.substr(delimPos + 1);

    if (!msg.empty() && msg[0] == ':')
        msg.erase(0, 1);
    
    std::string fullMsg = ":" + user.nickname() + "!" + user.username() + "@localhost PRIVMSG " + target + " :" + msg + "\r\n";

    if (target[0] == '#') {
        if (_channels.find(target) != _channels.end()) {
            _channels.find(target)->second.broadcast(fullMsg, user.socket());
            std::cout << "[Server] " << user.nickname() << " sent msg to " << target << std::endl;
        } else {
            user.writeBuffer() += ":ircserver 401 " + user.nickname() + " " + target + " :No such nick/channel\r\n";
        }
    } else {
        std::map<int, User>::iterator it = findUserNickname(target);
        if (it != _users.end()) {
            it->second.writeBuffer() += fullMsg;
            std::cout << "[Server] " << user.nickname() << " sent PM to " << target << std::endl;
        } else {
            std::string errMsg = ":ircserver 401 " + user.nickname() + " " + target + " :No such nick/channel\r\n";
            user.writeBuffer() += errMsg;
        }
    }
}

void Server::handleTopic(User& user, const std::string& args) {
    size_t delimPos = args.find(' ');
    std::string channel = (delimPos != std::string::npos) ? args.substr(0, delimPos) : args;
    std::string topic = (delimPos != std::string::npos) ? args.substr(delimPos + 1) : "";

    if (_channels.find(channel) == _channels.end()) {
        user.writeBuffer() += ":ircserver 403 " + user.nickname() + " " + channel + " :No such channel\r\n";
        return;
    }

    Channel& c = _channels.find(channel)->second;
    if (!c.isMember(user.socket())) {
        user.writeBuffer() += ":ircserver 442 " + user.nickname() + " " + channel + " :You're not on that channel\r\n";
        return;
    }

    if (topic.empty()) {
        if (c.getTopic().empty())
            user.writeBuffer() += ":ircserver 331 " + user.nickname() + " " + channel + " :No topic is set\r\n";
        else
            user.writeBuffer() += ":ircserver 332 " + user.nickname() + " " + channel + " :" + c.getTopic() + "\r\n";
    } else {
        if (c.isTopicRestricted() && !c.isOperator(user.socket())) {
            user.writeBuffer() += ":ircserver 482 " + user.nickname() + " " + channel + " :You're not channel operator\r\n";
            return;
        }
        if (topic[0] == ':') topic.erase(0, 1);
        c.setTopic(topic);
        
        std::string msg = ":" + user.nickname() + "!" + user.username() + "@localhost TOPIC " + channel + " :" + topic + "\r\n";
        c.broadcast(msg);
    }
}

void Server::handleKick(User& user, const std::string& args) {
    size_t firstDelim = args.find(' ');
    if (firstDelim == std::string::npos)
        return ;
    
    std::string channel = args.substr(0, firstDelim);
    size_t secondDelim = args.find(' ', firstDelim + 1);
    std::string target = (secondDelim != std::string::npos) ? args.substr(firstDelim + 1, secondDelim - firstDelim - 1) : args.substr(firstDelim + 1);
    std::string reason = (secondDelim != std::string::npos) ? args.substr(secondDelim + 1) : ":No reason given";

    if (_channels.find(channel) == _channels.end()) 
        return;
    Channel& c = _channels.find(channel)->second;

    if (!c.isMember(user.socket()))
        return;
    if (!c.isOperator(user.socket())) {
        user.writeBuffer() += ":ircserver 482 " + user.nickname() + " " + channel + " :You're not channel operator\r\n";
        return;
    }

    std::map<int, User>::iterator it = findUserNickname(target);
    if (it == _users.end() || !c.isMember(it->second.socket())) {
        user.writeBuffer() += ":ircserver 441 " + user.nickname() + " " + target + " " + channel + " :They aren't on that channel\r\n";
        return;
    }

    std::string kickMsg = ":" + user.nickname() + " KICK " + channel + " " + target + " " + reason + "\r\n";
    c.broadcast(kickMsg);
    c.removeMember(it->second.socket());
}

void Server::handleMode(User& user, const std::string &args) {
    std::istringstream iss(args);
    std::string channel, modeStr;
    iss >> channel >> modeStr;

    if (channel.empty() || channel[0] != '#') return;
    if (_channels.find(channel) == _channels.end()) {
        user.writeBuffer() += ":ircserver 403 " + user.nickname() + " " + channel + " :No such channel\r\n";
        return;
    }

    Channel& c = _channels.find(channel)->second;

    if (modeStr.empty()) {
        std::string modes = "+";
        if (c.isInviteOnly()) modes += "i";
        if (c.isTopicRestricted()) modes += "t";
        if (!c.getPassword().empty()) modes += "k";
        if (c.getLimit() > 0) modes += "l";
        user.writeBuffer() += ":ircserver 324 " + user.nickname() + " " + channel + " " + modes + "\r\n";
        return;
    }

    if (!c.isOperator(user.socket())) {
        user.writeBuffer() += ":ircserver 482 " + user.nickname() + " " + channel + " :You're not channel operator\r\n";
        return;
    }

    bool adding = true;
    for (size_t i = 0; i < modeStr.length(); ++i) {
        char m = modeStr[i];
        if (m == '+') { adding = true; continue; }
        if (m == '-') { adding = false; continue; }

        if (m == 'i') {
            c.setInviteOnly(adding);
        } else if (m == 't') {
            c.setTopicRestricted(adding);
        } else if (m == 'k') {
            std::string keyArg;
            if (adding) {
                if (iss >> keyArg) c.setPassword(keyArg);
            } else {
                c.setPassword("");
            }
        } else if (m == 'o') {
            std::string target;
            if (iss >> target) {
                std::map<int, User>::iterator it = findUserNickname(target);
                if (it != _users.end() && c.isMember(it->second.socket())) {
                    if (adding) c.addOperator(it->second.socket());
                    else c.removeOperator(it->second.socket());
                }
            }
        } else if (m == 'l') {
            if (adding) {
                size_t limitVal = 0;
                if (iss >> limitVal) c.setLimit(limitVal);
            } else {
                c.setLimit(0);
            }
        }
    }
    std::string modeMsg = ":" + user.nickname() + "!" + user.username() + "@localhost MODE " + channel + " " + modeStr + "\r\n";
    c.broadcast(modeMsg);
}

void Server::handleInvite(User& user, const std::string& args) {
    size_t delim = args.find(' ');
    if (delim == std::string::npos) {
        user.writeBuffer() += ":ircserver 461 " + user.nickname() + " INVITE :Not enough parameters\r\n";
        return;
    }

    std::string target = args.substr(0, delim);
    std::string channel = args.substr(delim + 1);

    if (_channels.find(channel) == _channels.end()) {
        user.writeBuffer() += ":ircserver 403 " + user.nickname() + " " + channel + " :No such channel\r\n";
        return;
    }

    Channel& c = _channels.find(channel)->second;
    if (!c.isMember(user.socket())) {
        user.writeBuffer() += ":ircserver 442 " + user.nickname() + " " + channel + " :You're not on that channel\r\n";
        return;
    }

    if (!c.isOperator(user.socket())) {
        user.writeBuffer() += ":ircserver 482 " + user.nickname() + " " + channel + " :You're not channel operator\r\n";
        return;
    }

    std::map<int, User>::iterator it = findUserNickname(target);
    if (it == _users.end()) {
        user.writeBuffer() += ":ircserver 401 " + user.nickname() + " " + target + " :No such nick/channel\r\n";
        return;
    }

    if (c.isMember(it->second.socket())) {
        user.writeBuffer() += ":ircserver 443 " + user.nickname() + " " + target + " " + channel + " :is already on channel\r\n";
        return;
    }

    c.inviteUser(target);
    user.writeBuffer() += ":ircserver 341 " + user.nickname() + " " + target + " " + channel + "\r\n";
    
    std::string inviteNotice = ":" + user.nickname() + "!" + user.username() + "@localhost INVITE " + target + " :" + channel + "\r\n";
    it->second.writeBuffer() += inviteNotice;
}

void Server::handlePing(User& user, const std::string& args) {
    user.writeBuffer() += ":ircserver PONG ircserver " + args + "\r\n";
}
