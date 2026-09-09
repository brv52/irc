#ifndef SOCKETGUARD_HPP
#define SOCKETGUARD_HPP

class SocketGuard {
    private:
        int _fd;

        SocketGuard(const SocketGuard&);
        SocketGuard& operator=(const SocketGuard&);
    public:
        SocketGuard();
        explicit SocketGuard(int fd);
        ~SocketGuard();

        int get() const;
        int release();
        void reset(int newFd);
};

#endif