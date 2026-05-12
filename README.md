![chat-banner](https://dummyimage.com/1200x280/1e293b/ffffff&text=OS-Level+Chat+Server+Using+Thread+Pools)

![Language Badge](https://img.shields.io/badge/Language-C-00599C?style=for-the-badge&logo=c)
![Threads Badge](https://img.shields.io/badge/Threads-POSIX_Pthreads-orange?style=for-the-badge)
![Networking Badge](https://img.shields.io/badge/Networking-TCP_Sockets-green?style=for-the-badge)
![Synchronization Badge](https://img.shields.io/badge/Synchronization-Mutexes_%26_Semaphores-red?style=for-the-badge)
![Architecture Badge](https://img.shields.io/badge/Architecture-Thread_Pool-blueviolet?style=for-the-badge)

# OS-Level Chat Server Using Thread Pools

## DESCRIPTION:
A multithreaded TCP chat server implemented in C using POSIX threads and socket programming.

The project combines several core Operating Systems concepts including thread pools, synchronization, producer-consumer queues, circular buffers, and file persistence into one complete real-time chat system.

---

## FEATURES:
- Fixed worker thread pool
- Concurrent multi-client support
- Room-based public chat
- Private messaging with `/msg`
- Nickname changing with `/nick`
- Room switching with `/join`
- Active room and user listings
- Token-based authentication
- Per-client rate limiting
- Persistent room history
- Server-side logging

---

## TECH STACK:
- C
- POSIX Threads (pthreads)
- TCP Socket Programming
- Mutexes & Semaphores
- Producer-Consumer Queue
- Circular Buffer
- File I/O

---

## BUILD:

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

## RUN:

Start the server:

```bash
./server
```

Connect client:

```bash
./interactive_client localhost 2000 Alice
```

---

## OPTIONAL AUTHENTICATION:

```bash
export CHAT_SERVER_TOKEN="your-token"
./server
```

Client authentication:

```bash
/auth your-token
```

---

## CLIENT COMMANDS:

| Command | Description |
|---|---|
| `/auth <token>` | Authenticate with server |
| `/nick <name>` | Change nickname |
| `/join <room>` | Switch room |
| `/msg <user> <text>` | Private message |
| `/users` | Show room users |
| `/rooms` | List active rooms |
| `/help` | Show commands |
| `/quit` | Disconnect |

---

## THREAD POOL ARCHITECTURE:

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

## HOW IT WORKS:

1. Server creates a listening TCP socket
2. Worker threads initialize at startup
3. Main thread accepts client connections
4. Client sockets enter bounded queue
5. Worker threads process client sessions
6. Messages are routed by room
7. Private messages are sent directly
8. Room history is stored in memory and on disk

---

## MESSAGE PERSISTENCE:

Messages are saved in:

```text
room_<name>.txt
```

Format:

```text
[roomname][username] message text
```

Example:

```text
[gaming][Alice] Hello everyone
[gaming][Abdul] Hi Alice
```

---

## LOGGING:

Server logs are stored in:

```text
chat_server.log
```

Includes:
- Connections
- Disconnects
- Authentication events
- Errors
- Server activity

---

## OPERATING SYSTEMS CONCEPTS USED:

| Concept | Usage |
|---|---|
| TCP Sockets | Client-server communication |
| POSIX Threads | Concurrent execution |
| Mutexes | Shared resource protection |
| Semaphores | Queue synchronization |
| Producer-Consumer | Task scheduling |
| Circular Buffers | Room history |
| File Persistence | Saved chats |

---

## FUTURE IMPROVEMENTS:
- GUI client
- Encrypted communication
- Database persistence
- WebSocket support
- Admin moderation tools
- File sharing
- Notifications
