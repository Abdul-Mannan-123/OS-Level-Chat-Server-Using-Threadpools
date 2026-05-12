# OS-Level Chat Server Using Thread Pools

<div align="center">

![C](https://img.shields.io/badge/Language-C-blue?style=flat-square&logo=c)
![Threads](https://img.shields.io/badge/Pthreads-Thread%20Pool-orange?style=flat-square)
![Sockets](https://img.shields.io/badge/TCP-Sockets-green?style=flat-square)
![OS](https://img.shields.io/badge/Operating%20Systems-Project-red?style=flat-square)

A multithreaded TCP chat server written in C using POSIX threads, sockets, semaphores, mutexes, and file persistence.

</div>

---

## Overview

This project is a multithreaded socket-based chat server built in C.  
The server uses a fixed-size worker thread pool to efficiently handle multiple TCP client connections concurrently.

The system supports:

- Room-based public chat
- Private messaging
- Thread-pool concurrency
- File-based message persistence
- Authentication
- Rate limiting
- Server logging

The project combines several core Operating Systems concepts into one complete working system.

---

## Features

- Fixed thread pool with 4 worker threads
- Concurrent TCP client handling
- Room-based public chat
- Private messaging with `/msg`
- Nickname changes with `/nick`
- Room switching with `/join`
- Room and user listings
- Optional token-based authentication
- Per-client rate limiting
- Persistent room history
- Server-side logging

---

## Operating Systems Concepts Used

| Concept | Usage |
|---|---|
| TCP Socket Programming | Client-server communication |
| POSIX Threads | Concurrent client handling |
| Mutexes & Semaphores | Synchronization |
| Producer-Consumer Queue | Worker task scheduling |
| Circular Buffers | Room history storage |
| File Persistence | Saved room chats |

---

## Repository Structure

```text
os-chat-server/
│
├── server.c
├── interactive_client.c
├── data_structs.c
├── data_structs.h
├── socket_utils.c
├── socket_utils.h
├── Makefile
├── PROJECT_REPORT.txt
├── chat_server.log
└── room_<name>.txt
```

---

## Build

Compile the project using:

```bash
make all
```

Generated binaries:

```text
server
interactive_client
```

Clean build files:

```bash
make clean
```

---

## Running the Project

Start the server:

```bash
./server
```

The server listens on port:

```text
2000
```

Connect clients:

```bash
./interactive_client localhost 2000 Alice
```

Example:

```bash
./interactive_client localhost 2000 Abdul
```

---

## Optional Authentication

Enable authentication using an environment variable:

```bash
export CHAT_SERVER_TOKEN="your-token"
./server
```

Authenticate from client:

```bash
/auth your-token
```

---

## Client Commands

| Command | Description |
|---|---|
| `/auth <token>` | Authenticate with server |
| `/nick <name>` | Change nickname |
| `/join <room>` | Switch rooms |
| `/msg <user> <text>` | Send private message |
| `/users` | Show users in room |
| `/rooms` | List active rooms |
| `/help` | Show commands |
| `/quit` | Disconnect |

Any line not starting with `/` is treated as a public room message.

---

## Thread Pool Architecture

```text
                +------------------+
                |   Main Thread    |
                | Accept Clients   |
                +--------+---------+
                         |
                         v
               +-------------------+
               |   Bounded Queue   |
               | Producer/Consumer |
               +---------+---------+
                         |
        +----------------+----------------+
        |                |                |
        v                v                v
   +---------+     +---------+     +---------+
   |Worker 1 |     |Worker 2 |     |Worker 3 |
   +---------+     +---------+     +---------+
                         |
                         v
                    +---------+
                    |Worker 4 |
                    +---------+
```

---

## How It Works

1. The server creates a listening TCP socket.
2. Four worker threads are created at startup.
3. The main thread accepts incoming client sockets.
4. Accepted sockets are inserted into a bounded queue.
5. Worker threads dequeue sockets and process clients.
6. Commands are parsed and executed.
7. Public messages are broadcast to room members.
8. Private messages are routed directly to recipients.
9. Room history is stored in memory and on disk.

---

## Message Persistence

Each room maintains:

- In-memory circular message history
- Persistent room log files

Stored message format:

```text
[roomname][username] message text
```

Example:

```text
[gaming][Alice] Hello everyone
[gaming][Abdul] Hi Alice
```

Room history is automatically reloaded after server restart.

---

## Logging

Server activity is written to:

```text
chat_server.log
```

Logs include:

- Client connections
- Disconnects
- Authentication events
- Errors
- General server activity

---

## Example Session

### Terminal 1

```bash
./server
```

### Terminal 2

```bash
./interactive_client localhost 2000 Alice

/join gaming
Hello from gaming!
```

### Terminal 3

```bash
./interactive_client localhost 2000 Abdul

/join gaming
Hi Alice
```

---

## Implementation Notes

### Thread Pool Design

Instead of creating one thread per client, the server uses:

```text
Fixed Worker Pool + Shared Queue
```

Benefits:

- Lower thread creation overhead
- Better scalability
- Controlled resource usage
- Improved responsiveness

---

### Synchronization

Shared resources are protected using:

- `pthread_mutex_t`
- POSIX semaphores

Used for:

- Queue synchronization
- Room management
- Shared client data
- Message history protection

---

## Technologies Used

| Technology | Purpose |
|---|---|
| C | Core implementation |
| POSIX Threads | Multithreading |
| TCP Sockets | Networking |
| Mutexes | Synchronization |
| Semaphores | Producer-consumer control |
| File I/O | Persistence |

---

## Learning Outcomes

This project demonstrates practical understanding of:

- Operating Systems
- Concurrent programming
- Networking fundamentals
- Synchronization
- Thread-pool architecture
- Producer-consumer systems
- Socket communication
- Persistent server design

---

## Future Improvements

- GUI client
- Encrypted communication
- Database-backed persistence
- WebSocket support
- Admin moderation tools
- File sharing
- Notifications

---

<div align="center">

Built using C, POSIX Threads, and TCP Sockets.

</div>
