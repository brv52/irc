#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <vector>
#include <poll.h>
#include <map>
#include "User.hpp"
#include "Channel.hpp"

extern bool g_serverRunning;

class Server {
    private:
        int _socket;
        int _port;
        std::string _password;
        std::vector<pollfd> _connections;
        std::map<int, User> _users;
        std::map<std::string, Channel> _channels;

        Server();
        Server(const Server& src);
        Server& operator=(const Server& rhs);

        void acceptUser();
        bool readData(int socket);
        bool sendData(int socket);
        void processCommand(User& user, const std::string& cmd);

        void handleJoin(User& iser, const std::string& args);
        void handleMsg(User& user, const std::string& args);
        std::map<int, User>::iterator findUserNickname(const std::string& target);

        void handleTopic(User& user, const std::string& args);
        void handleKick(User& user, const std::string& args);
        void handleMode(User& user, const std::string &args);
        void handleInvite(User& user, const std::string& args);
        void handlePing(User& user, const std::string& args);
    public:
        Server(int port, const std::string& password);
        ~Server();
        void run();
};

#endif