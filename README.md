# IRC Server

A lightweight IRC-style chat server implemented in modern C++ using POSIX sockets and a non-blocking event loop. The project follows the core architecture of a real-time multi-client chat backend, including user registration, channel management, command processing, and message broadcasting.

## Project Overview

This repository contains a compact IRC server that supports a subset of IRC behavior and channel operations. It is designed as a network service that accepts multiple client connections concurrently, validates registration, and routes messages and commands across users and channels.

The implementation is intentionally focused on the foundational networking concepts required in systems programming:

- multi-client socket handling
- non-blocking I/O with `poll()`
- user and channel state management
- IRC command parsing and validation
- operator controls for channel moderation

The server is built as a single executable, `ircserv`, and is intended to be run locally or on a Linux-based environment with a standard C++ toolchain.

## Core Architecture & Technical Implementation

### 1. Network layer

The server is built on the POSIX socket API and runs a listening TCP socket bound to a configurable port.

Key implementation notes:

- `Server` owns the listening socket and maintains the active client set.
- `SocketGuard` provides RAII-based lifecycle management for socket file descriptors.
- `fcntl(..., F_SETFL, O_NONBLOCK)` is used to place both the listening socket and client sockets into non-blocking mode.
- `poll()` is used in a single-threaded readiness loop to monitor all connected sockets.

This is a classic non-blocking event-driven server pattern: the process wakes when any socket is readable or writable, then processes only the ready sockets without blocking on individual clients.

### 2. Event loop and I/O model

The main loop in `Server::run()` performs the following actions:

1. updates the `pollfd` entries for each connected user
2. marks sockets as writable when outbound data exists in the user write buffer
3. calls `poll()` with an indefinite timeout
4. accepts new connections when the listening socket is readable
5. reads incoming data with `recv()`
6. parses IRC commands line by line using `\r\n` delimiters
7. writes queued server responses with `send()` only when the socket is ready

This model makes the service resilient to slow or bursty clients and is a good match for the requirements of a real IRC daemon.

### 3. User and registration model

Each connected client is represented by a `User` object containing:

- socket descriptor
- read buffer and write buffer
- nickname and username
- login state and registration state

The server enforces a registration flow similar to IRC:

- `PASS` must be accepted first
- `NICK` and `USER` complete the client identity
- a user is only considered fully registered after both login and identity fields are set

This validation logic is centralized in the command processing path inside `Server::processCommand()`.

### 4. Channel management

Channels are modeled by the `Channel` class and stored in a map keyed by channel name.

Supported channel behavior includes:

- joining and leaving members
- operator assignment and privilege validation
- broadcast messaging to all members
- topic management
- invite-only mode
- channel password protection
- user limit enforcement

The server uses `Channel::broadcast()` to distribute messages efficiently to all channel members and supports operator-only commands such as `MODE`, `KICK`, and `TOPIC` changes when appropriate.

### 5. Command handling

The server parses commands such as:

- `PASS`
- `NICK`
- `USER`
- `JOIN`
- `PRIVMSG`
- `PING`
- `KICK`
- `INVITE`
- `TOPIC`
- `MODE`
- `QUIT`

Each command is processed through a dedicated handler and responds with IRC-like numeric codes such as `401`, `403`, `431`, `451`, `462`, `471`, `473`, `475`, and others.

### 6. Operational notes

This project is a standalone native C++ server and does not currently include Docker Compose or container orchestration files. The runtime model is a single-process Linux service rather than a microservice-based deployment.

## Prerequisites

Before building and running the project, ensure the following are installed on a Linux environment:

- GCC or Clang compatible with C++98
- GNU Make
- standard POSIX development headers (`sys/socket.h`, `netinet/in.h`, etc.)
- optional: `nc` or similar TCP client for testing

### Required environment

This project is intended for Unix-like systems and is tailored to Linux socket APIs. It is not intended for native Windows builds without additional compatibility layers.

## Build Instructions

The repository provides a `Makefile` with standard targets.

### Build the project

```bash
make
```

This produces the executable:

```bash
./ircserv
```

### Clean build artifacts

```bash
make clean
```

### Full rebuild

```bash
make re
```

This removes the compiled objects and executable and rebuilds from scratch.

### Build configuration details

The `Makefile` compiles with:

- `-Wall`
- `-Wextra`
- `-Werror`
- `-std=c++98`

This keeps the project aligned with the strict constraints of the original low-level C++ assignment environment.

## Usage

### Start the server

```bash
./ircserv 6667 mypassword
```

- port must be a valid integer between 1024 and 65535
- password is required and must not contain whitespace

### Connect with a client

Using `nc`:

```bash
nc 127.0.0.1 6667
```

Then send IRC-style commands:

```text
PASS mypassword
NICK alice
USER alice 0 * :Alice Example
JOIN #general
PRIVMSG #general :Hello everyone!
PING :server
QUIT
```

### Example interaction

```text
PASS mypassword
NICK alice
USER alice 0 * :Alice Example
JOIN #general
PRIVMSG #general :Hello from alice!
MODE #general +t
TOPIC #general :Team discussion
```

### Test script

The repository includes a test harness, `test_suite.sh`, which exercises common networking and IRC behavior scenarios, including:

- fragmented messages
- authorization checks
- malformed commands
- flood resilience
- channel permission handling

Run it with:

```bash
bash test_suite.sh
```

> Note: the script assumes the server is already running locally on `127.0.0.1:2222` and uses a default password value defined in the script.

## Project Structure

```text
.
├── Channel.cpp
├── Channel.hpp
├── main.cpp
├── Makefile
├── README.md
├── Server.cpp
├── Server.hpp
├── SocketGuard.cpp
├── SocketGuard.hpp
├── User.cpp
├── User.hpp
├── test_suite.sh
```

## Summary

This project demonstrates a solid systems-programming approach to building an IRC-style server in C++. It combines low-level networking, event-driven input handling, and robust command parsing into a compact yet production-oriented codebase. The architecture is especially relevant for roles involving backend engineering, network systems, real-time messaging platforms, and C++ server development.

The implementation is intentionally small, efficient, and educational, while still showcasing the core pattern behind real multi-client network services.
