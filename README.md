# spectre
minimal implementation of Discord Bot API

# Requirements
[nlohmann/json](https://github.com/nlohmann/json)
[OpenSSL C Libraries](https://github.com/openssl/openssl)

# compilation
```
g++ main.cc -o main -lssl -lcrypto -g
```
# Event Payloads Captured
-   Interaction Create 
-   Message Create 
-   Voice State Update

# Architecture
Note: non-blocking sockets are necessary here for heartbeats

Low Level Architecture (Single Threaded)
    -   Standard socket for the WebSocket connection
        -   Data arriving from Discord arrives here
        -   Requires a check for readable (When data arrives) and writable (When data is ready to be sent)
    -   timerfd for the heartbeat interval
        -   Sends a notification when timer expires
    -   eventfd for when an event is ready to be handled (initiated by user side)
        -   Unlikely to be the case since events are first ack by Discord before response

Higher Level Architecture:
-   Class that defines a bot object, then to use it you must create a class that inherits from the bot object with your own initialization.

Boot Process:
    -   First grab valid wss:// url from gateway (GET REQUEST);
        -   Get Gateway Bot requires authentication field using (BOT TOKEN)
        -   See: [API REFERENCE](https://docs.discord.com/developers/reference)

# Reading
It is possible to use OpenSSL BIO libraries to do address lookup, see [here](https://docs.openssl.org/master/man7/ossl-guide-tls-client-block/#creating-the-socket-and-bio)

# TODO 
-   [ ] Use Get Gateway Bot instead of Get Gateway (Boot Process)
-   [ ] Change relevant std::size_t lens to initialize to std::string::npos
-   [ ] Use autotools to make a compilation script
-   [ ] Remember to free relevant items when done
-   [ ] addrinfo struct (linked list) needs to be freed after address is obtained
-   [ ] Think of a better class name (rather than bot)
-   [ ] Set up a logging fd so log events trigger an event in epoll to print something during the infinite loop
-   [ ] Refactor socket creation and ssl setup so that there is less code repetition
-   [ ] Replace relevant couts with cerr
-   [x] Abstract basic select() waiting (needed for handshake and shutdown)
-   [ ] Replace relevant bools with enums
-   [ ] handle_io_errors() should return enum not int

# Notes
- string_view is faster than std::string& so use string_view for read-only operations
- CRLF separates header and payload (we assume discord api behaves nicely)
- Each line ends with a CRLF, whereas header ends with CRLFCRLF
- Content-Length int contans \n at end of payload
