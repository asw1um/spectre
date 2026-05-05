# spectre
minimal implementation of Discord Bot API

# Requirements
[nlohmann/json](https://github.com/nlohmann/json)
[OpenSSL C Libraries](https://github.com/openssl/openssl)

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
    - Used for log events

Higher Level Architecture:
-   Class that defines a bot object, then to use it you must create a class that inherits from the bot object with your own initialization.

# Reading
It is possible to use OpenSSL BIO libraries to do address lookup, see [here](https://docs.openssl.org/master/man7/ossl-guide-tls-client-block/#creating-the-socket-and-bio)

# TODO 
-   [ ] Use Get Gateway Bot instead of Get Gateway (Boot Process)
-   [ ] Remember to free relevant items when done
-   [ ] addrinfo struct (linked list) needs to be freed after address is obtained
-   [ ] Refactor socket creation and ssl setup so that there is less code repetition
-   [ ] Replace relevant bools with enums
-   [ ] Refactor set_ws_url() and form() member functions to be more concise
-   [ ] Make while read logic more understandble
-   [ ] Incorporate data_frame class which is built over time rather than an outright constructor (Perhaps a struct here is better)
-   [ ] Generalize error handling with custom logger
-   [ ] Move all necessary events to Main Read Loop
-   [ ] Refactor handle_io_errors 
-   [ ] Implement throwing exceptions for _frame classes
-   [ ] Consider building payload in _frame classes rather than passing as arg
-   [ ] payload_length can be implicitly determined for frames rather than explicitly given as arg
-   [ ] Separate Code for Initial Get Request, WS Upgrade, and Main WS Loop
-   [ ] Switch all variable declarations to auto = type-id {} style to avoid undefined behaviour
-   [ ] Make a more robust bit class to handle the operations being done for the data frames
-   [ ] Rename SPCTR_ERROR enum class
-   [ ] Make log queue thread-safe
-   [ ] Make calls to write to eventfd safe
-   [ ] Replace std::cout with log events that work with errno
-   [ ] Code that builds frame could be abstracted to data frame abstract class
-   [ ] Check if Payload Length checks are necessary for spectre payloads (they are 100% necessary for user payloads)
-   [ ] Implement Intents Enum
-   [ ] Have user supply Enum vs hardcoding it

# Notes
- string_view is faster than std::string& so use string_view for read-only operations
- CRLF separates header and payload (we assume discord api behaves nicely)
- Each line ends with a CRLF, whereas header ends with CRLFCRLF
- Content-Length int contans \n at end of payload

## Websocket Protocol Notes
[Source](https://datatracker.ietf.org/doc/html/rfc6455#section-5)
-   All data frames sent from clients must be masked
    - If a server is sent an unmasked data frame the server must close the connection
-   A server MUST NOT mask any frames that it sendsd to the clients 
    - A client MUST close a connection if it detects a masked frame
-   Data is in Big Endian Format
