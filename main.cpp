#include <sys/socket.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include <stdlib.h>
#include "Server.hpp"
#include <csignal>

bool g_serverRunning = true;

void signalHandler(int signum) {
    (void)signum;
    g_serverRunning = false;
    std::cout << "\n[Server] Shutting down..." << std::endl;
}

int parsePort(const char* rawPort) {
    if (!rawPort || *rawPort == '\0')
        throw std::invalid_argument("Port is empty");
    for (int i = 0; rawPort[i]; ++i) {
        if (!std::isdigit(rawPort[i]))
            throw std::invalid_argument("Port must contain only digits");
    }
    long port = strtol(rawPort, NULL, 10);
    if (port < 1024 || port > 65535)
        throw std::invalid_argument("Invalid port: must be in <1024, 65535>");
    return static_cast<int>(port);
}

std::string parsePassword(const char* rawPassword) {
    if (!rawPassword || *rawPassword == '\0')
        throw std::invalid_argument("Password is empty");
    std::string password(rawPassword);
    if (password.empty())
        throw std::invalid_argument("Password cannot be empty");
    if (password.find_first_of(" \t\r\n") != std::string::npos)
        throw std::invalid_argument("Password must not contain whitespaces");
    return password;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./ircserv [port] [password]" << std::endl;
        return 1;
    }
    try {
        int port = parsePort(argv[1]);
        std::string password = parsePassword(argv[2]);
        signal(SIGINT, signalHandler);
        signal(SIGQUIT, signalHandler);
        Server server(port, password);
        server.run();
    }
    catch (const std::invalid_argument& e) {
        std::cerr << "Configuration error: " << e.what() << std::endl;
        return 1;
    } catch (const std::runtime_error& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << std::endl;
        return 1;
    }

    return (0);
}