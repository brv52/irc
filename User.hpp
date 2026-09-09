#ifndef USER_HPP
#define USER_HPP

#include <string>

class User {
    private:
        int _socket;
        std::string _rBuffer;
        std::string _wBuffer;
        std::string _nickname;
        std::string _username;
        bool _loggedIn;
        bool _registered;
    public:
        User();
        User(const User& src);
        User& operator=(const User& rhs);
        User(int fd);
        ~User();

        int socket();

        std::string& readBuffer();
        std::string& writeBuffer();
        
        std::string& nickname();
        std::string& username();
        bool loggedIn();
        void logIn();
        bool registered();
        void reg();
};

#endif