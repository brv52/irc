#include "Channel.hpp"
#include <algorithm>

Channel::Channel(const std::string& name) 
:   _name(name), _topic(""),
    _password(""), _userLimit(0),
    _inviteOnly(false), _topicOpOnly(true) 
{}

Channel::~Channel() {}

Channel::Channel(const Channel& src) {
    *this = src;
}

Channel& Channel::operator=(const Channel& rhs) {
    if (this != &rhs) {
        _name = rhs._name;
        _members = rhs._members;
        _operators = rhs._operators;
        _topic = rhs._topic;
        _password = rhs._password;
        _userLimit = rhs._userLimit;
        _inviteOnly = rhs._inviteOnly;
        _topicOpOnly = rhs._topicOpOnly;
        _invitedUsers = rhs._invitedUsers;
    }
    return *this;
}

const std::string& Channel::getName() const {
    return _name;
}

bool Channel::isEmpty() const {
    return _members.empty();
}

void Channel::addMember(User* user, bool isOperator) {
    if (!user)
        return;

    _members[user->socket()] = user;
    if (isOperator)
        _operators[user->socket()] = user;
}

void Channel::removeMember(int socket) {
    _members.erase(socket);
    _operators.erase(socket);
}

void Channel::broadcast(const std::string& msg, int exceptSocket) {
    std::map<int, User*>::iterator it;
    for (it = _members.begin(); it != _members.end(); ++it) {
        if (it->first != exceptSocket)
            it->second->writeBuffer() += msg;
    }
}

std::string Channel::getNames() const {
    std::string res = "";
    std::map<int, User*>::const_iterator cit;
    for (cit = _members.begin(); cit != _members.end(); ++cit) {
        if (_operators.find(cit->first) != _operators.end())
            res += "@";
        res += cit->second->nickname() + " ";
    }
    return res;
}

bool Channel::isOperator(int socket) const {
    return (_operators.find(socket) != _operators.end());
}

bool Channel::isMember(int socket) const {
    return (_members.find(socket) != _members.end());
}

const std::string& Channel::getTopic() const {
    return _topic;
}

void Channel::setTopic(const std::string& topic) {
    _topic = topic;
}

bool Channel::isInviteOnly() const {
    return _inviteOnly;
}

void Channel::setInviteOnly(bool val) {
    _inviteOnly = val;
}

const std::string& Channel::getPassword() const {
    return _password;
}

void Channel::setPassword(const std::string& pswd) {
    _password = pswd;
}

size_t Channel::getLimit() const {
    return _userLimit;
}

void Channel::setLimit(size_t limit) {
    _userLimit = limit;
}

void Channel::addOperator(int socket) {
    if (isMember(socket))
        _operators[socket] = _members.find(socket)->second;
}

void Channel::removeOperator(int socket) {
    _operators.erase(socket);
}

void Channel::inviteUser(const std::string& nickname) {
    if (!isInvited(nickname))
        _invitedUsers.push_back(nickname);
}

bool Channel::isInvited(const std::string& nickname) const {
    return (std::find(_invitedUsers.begin(), _invitedUsers.end(), nickname) != _invitedUsers.end());
}

void Channel::removeInvite(const std::string& nickname) {
    std::vector<std::string>::iterator it = std::find(_invitedUsers.begin(), _invitedUsers.end(), nickname);
    if (it != _invitedUsers.end())
        _invitedUsers.erase(it);
}

bool Channel::isTopicRestricted() const {
    return _topicOpOnly;
}
void Channel::setTopicRestricted(bool val) {
    _topicOpOnly = val;
}

size_t Channel::getMembersCount() const {
    return _members.size();
}