#ifndef CHANNEL_HPP
#define CHANNEL_HPP

#include <string>
#include <map>
#include <vector>
#include "User.hpp"

class Channel {
    private:
        std::string _name;
        std::map<int, User*> _members;
        std::map<int, User*> _operators;

        std::string _topic;
        std::string _password;
        size_t _userLimit;
        bool _inviteOnly;
        bool _topicOpOnly;

        std::vector<std::string> _invitedUsers;

        Channel();
    public:
        Channel(const std::string& name);
        ~Channel();
        Channel(const Channel& src);
        Channel& operator=(const Channel& rhs);

        const std::string& getName() const;
        bool isEmpty() const;

        void addMember(User* user, bool isOperator);
        void removeMember(int socket);

        void broadcast(const std::string& msg, int exceptSocket = -1);
        std::string getNames() const;

        bool isOperator(int socket) const;
        bool isMember(int socket) const;
        const std::string& getTopic() const;
        void setTopic(const std::string& topic);
        bool isInviteOnly() const;
        void setInviteOnly(bool val);
        const std::string& getPassword() const;
        void setPassword(const std::string& pswd);
        size_t getLimit() const;
        void setLimit(size_t limit);
        void addOperator(int socket);
        void removeOperator(int socket);
        void inviteUser(const std::string& nickname);
        bool isInvited(const std::string& nickname) const;
        void removeInvite(const std::string& nickname);
        bool isTopicRestricted() const;
        void setTopicRestricted(bool val);
        size_t getMembersCount() const;
};

#endif