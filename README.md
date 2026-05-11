# OS-Level Chat Server Using Thread Pools

## Project Overview

This project is a multithreaded, socket-based chat server written in C. It uses a fixed worker-pool design to accept multiple TCP client connections, route messages by chat room, and support private messaging between users. The server keeps recent room history in memory and persists chat logs to disk so room conversations remain available across restarts.

The project combines several operating-systems concepts in one working system:

- TCP socket programming
- POSIX threads
- mutexes and semaphores for synchronization
- bounded producer-consumer task handling
- circular buffers for room history
- file-based persistence for room messages

## Features

- Fixed server-side thread pool with 4 workers
- Concurrent client connection handling
- Room-based public chat
- Private messaging with `/msg`
- Nickname changes with `/nick`
- Room switching with `/join`
- Active room and user listings with `/rooms` and `/users`
- Optional token-based authentication through `CHAT_SERVER_TOKEN`
- Per-client rate limiting for plain chat messages
- Persistent room history in `room_<name>.txt` files
- Server logging to `chat_server.log`

## Build

Use the provided Makefile:

```bash
make all
```

This builds:

- `server`
- `interactive_client`

To clean build outputs:

```bash
make clean
```

## Run

Start the server first:

```bash
./server
```

Open one or more client terminals and connect with a nickname:

```bash
./interactive_client localhost 2000 YourName
```

The server listens on port `2000` by default.

If authentication is enabled on the server, set the token before starting it:

```bash
export CHAT_SERVER_TOKEN="your-token"
./server
```

## Client Commands

- `/auth <token>` authenticate with the server when a token is required
- `/nick <name>` change your display name
- `/join <room>` switch to a room and load its history
- `/msg <user> <text>` send a private message
- `/users` list users in the current room
- `/rooms` list active rooms
- `/help` show available commands
- `/quit` disconnect cleanly

Any line that does not start with `/` is treated as a public chat message for the current room.

## Repository Layout

- `server.c`: main server logic, thread pool, room routing, rate limiting, authentication, and logging
- `interactive_client.c`: terminal client used to connect and chat
- `data_structs.c` and `data_structs.h`: queue and supporting data structures
- `socket_utils.c` and `socket_utils.h`: socket helper functions
- `Makefile`: build instructions
- `PROJECT_REPORT.txt`: project report

## How It Works

1. The server creates a listening TCP socket.
2. Four worker threads are created at startup.
3. The main thread accepts incoming client sockets.
4. Accepted sockets are placed into a bounded queue.
5. Worker threads dequeue sockets and process each client session.
6. Commands are parsed and handled immediately.
7. Public messages are broadcast to everyone in the same room.
8. Private messages are delivered directly to the named recipient.
9. Room history is stored in memory and appended to disk.

The design keeps the server responsive while multiple clients are active at the same time.

## Message History and Persistence

Each room maintains a recent history in memory using a circular buffer. When a room message is added, it is also appended to the matching disk file using the format:

```text
[roomname][username] message text
```

When a room is opened again, its saved history is loaded back into memory so new users can see previous messages.

## Example Session

Terminal 1:

```bash
./server
```

Terminal 2:

```bash
./interactive_client localhost 2000 Alice
/join gaming
Hello from gaming!
```

Terminal 3:

```bash
./interactive_client localhost 2000 Abdul
/join gaming
Hi Bazil
```

## Implementation Notes

- The server uses a fixed thread pool instead of creating a new thread for every connection.
- Shared data structures are protected with mutexes and semaphores.
- The client is terminal-based and reads/writes plain text over TCP.
- Room files and the server log are stored in the project directory.

## Submission Notes

For submission, include the source files, Makefile, this README, and the project report.
