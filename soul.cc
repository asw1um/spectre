#include "spectre.h"
#include <cstddef>
#include <openssl/ssl.h>
#include <string_view>
#include <sys/epoll.h>

using SOCKET = int;
using namespace spctr;
using namespace nlohmann;

soul::soul(std::string_view i_bot_token, INTENTS i_intents) : log_instance()
{
        this->intents = i_intents;
        this->bot_token = i_bot_token;
        SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
        if (ctx == NULL)
        {
                std::cout << "Failed to create the SSL_CTX\n";
                exit(EXIT_FAILURE);
        }

        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

        if (!SSL_CTX_set_default_verify_paths(ctx))
        {
                std::cout << "Failed to set the default trusted certificate store\n";
                exit(EXIT_FAILURE);
        }

        if (!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION))
        {
                std::cout << "Failed to set the minimum TLS protocol version\n"; 
                exit(EXIT_FAILURE);
        }

        SSL *ssl = SSL_new(ctx);
        if (ssl == NULL)
        {
                std::cout << "Failed to create the SSL object\n";
                exit(EXIT_FAILURE);
        }

        int getaddrinfo_status = -1;
        struct addrinfo hints{};
        hints.ai_family = AF_INET;              // care about IPV4 only
        hints.ai_socktype = SOCK_STREAM;        // socket is TCP
        hints.ai_flags = AI_PASSIVE;

        struct addrinfo *servinfo;
        getaddrinfo_status = getaddrinfo("discord.com", "https", &hints, &servinfo);
        if (getaddrinfo_status != 0)
        {
                std::cerr << "getaddrinfo: " << gai_strerror(getaddrinfo_status) << "\n";
                exit(EXIT_FAILURE);
        }

        SOCKET main_sock = -1;
        for (struct addrinfo *p = servinfo; p != NULL; p = p->ai_next)
        {
                using namespace std;
                main_sock = socket(p->ai_family, p->ai_socktype, 0);
                if (main_sock == -1)
                {
                        cerr << "Failed to create socket.\n";
                        exit(EXIT_FAILURE);
                }

                int conn_status = connect(main_sock, p->ai_addr, p->ai_addrlen);
                if (conn_status != 0) 
                {
                        close(main_sock);
                        main_sock = -1;
                        continue;
                }
                break; 
        } 

        BIO *bio;
        bio = BIO_new(BIO_s_socket());
        if (bio == NULL)
        {
                BIO_closesocket(main_sock);
                exit(EXIT_FAILURE);
        }

        BIO_set_fd(bio, main_sock, BIO_CLOSE);

        SSL_set_bio(ssl, bio, bio);

        if (!SSL_set_tlsext_host_name(ssl, "discord.com"))
        {
                std::cerr << "Failed to set the SNI hostname\n";
                exit(EXIT_FAILURE);
        }

        if (!SSL_set1_host(ssl, "discord.com"))
        {
                std::cerr << "Failed to set the certificate verification hostname\n";
                exit(EXIT_FAILURE);
        }

        int ssl_conn_status = SSL_connect(ssl);
        if (ssl_conn_status < 1)
        {
                std::cerr << "Failed to connect to the server\n";
                if (SSL_get_verify_result(ssl) != X509_V_OK)
                {
                        std::cerr << "Verify error: " << X509_verify_cert_error_string(SSL_get_verify_result(ssl)) << "\n";
                }
                exit(EXIT_FAILURE);
        }

        size_t written;
        const char *req = "GET /api/v10/gateway HTTP/1.1\r\nHost: discord.com\r\nUser-Agent: SPECTRE/1.0.0\r\nAccept: */*\r\nConnection: close\r\n\r\n\r\n";
        if (!SSL_write_ex(ssl, req, strlen(req), &written))
        {
                std::cerr << "Failed to write HTTP request\n";
                exit(EXIT_FAILURE);
        }

        size_t read_bytes;
        char buf[64];
        std::string request;

        bool PAYLOAD_FOUND = false;
        std::size_t payload_len = std::string::npos;
        std::string payload;
        while(SSL_read_ex(ssl, buf, sizeof(buf), &read_bytes)) 
        {
                if (PAYLOAD_FOUND) continue;
                request.append(buf, read_bytes);
                std::string_view sv{request};
                std::size_t content_length_pos = sv.find("Content-Length: ");

                // We need eol_escape to ensure that we actuall have a number in the current sv
                std::size_t eol_escape = sv.find("\r", content_length_pos);

                if (content_length_pos != std::string::npos && eol_escape != std::string::npos) 
                {
                        payload_len = get_content_length(sv.substr(content_length_pos, eol_escape - content_length_pos));
                }                

                std::size_t payload_pos = sv.find("\r\n\r\n");
                if (payload_pos != std::string::npos && (payload_pos + 4) < sv.length()) 
                {
                        payload_pos += 4;
                        std::size_t substr_len = sv.substr(payload_pos, sv.length()).length();
                        if (substr_len == payload_len) 
                        {
                                payload = sv.substr(payload_pos, sv.find_last_of("}", payload_pos));
                                PAYLOAD_FOUND = true;
                        }                        
                }
        }

        // std::cerr print may be unnecessary here
        if (SSL_get_error(ssl, 0) != SSL_ERROR_ZERO_RETURN) std::cerr << "Failed reading remaining data\n";

        int ret = SSL_shutdown(ssl);
        if (ret < 1)
        {
                std::cerr << "Error shutting down\n";
                exit(EXIT_FAILURE);
        }
        SSL_free(ssl);
        SSL_CTX_free(ctx);

        auto j = json::parse(payload);
        std::string temp = j["url"];
        std::string_view sv(temp);
        this->ws_url = sv;
}

void soul::log(std::string_view output, LOG_LEVEL log_level)
{
        this->log_instance.log(output, log_level);
}

soul::~soul()
{
        std::cout << "Destroying soul object.\n";
}

void soul::form()
{
        std::string ws_host_name = this->ws_url;
        extract_host_name(ws_host_name);

        int getaddrinfo_status = -1;
        struct addrinfo hints{};
        hints.ai_family = AF_INET;              // care about IPV4 only
        hints.ai_socktype = SOCK_STREAM;        // socket is TCP

        struct addrinfo *servinfo;
        getaddrinfo_status = getaddrinfo(ws_host_name.data(), "https", &hints, &servinfo);
        if (getaddrinfo_status != 0)
        {
                std::cerr << "getaddrinfo: " << gai_strerror(getaddrinfo_status) << "\n";
                exit(EXIT_FAILURE);
        }

        SOCKET main_sock = -1;
        for (struct addrinfo *p = servinfo; p != NULL; p = p->ai_next)
        {
                using namespace std;
                main_sock = socket(p->ai_family, p->ai_socktype | SOCK_NONBLOCK, 0);
                if (main_sock == -1)
                {
                        cerr << "Failed to create socket.\n";
                        exit(EXIT_FAILURE);
                }

                int conn_status = connect(main_sock, p->ai_addr, p->ai_addrlen);
                if (conn_status != 0 && errno != EINPROGRESS) 
                {
                        close(main_sock);
                        main_sock = -1;
                        continue;
                }
                break; 
        } 

        SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
        if (ctx == NULL)
        {
                std::cout << "Failed to create the SSL_CTX\n";
                exit(EXIT_FAILURE);
        }

        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

        if (!SSL_CTX_set_default_verify_paths(ctx))
        {
                std::cout << "Failed to set the default trusted certificate store\n";
                exit(EXIT_FAILURE);
        }

        if (!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION))
        {
                std::cout << "Failed to set the minimum TLS protocol version\n";
                exit(EXIT_FAILURE);
        }

        SSL *ssl = SSL_new(ctx);
        if (ssl == NULL)
        {
                std::cout << "Failed to create the SSL object\n";
                exit(EXIT_FAILURE);
        }

        BIO *bio;
        bio = BIO_new(BIO_s_socket());
        if (bio == NULL)
        {
                BIO_closesocket(main_sock);
                exit(EXIT_FAILURE);
        }

        BIO_set_fd(bio, main_sock, BIO_CLOSE);
        SSL_set_bio(ssl, bio, bio);

        if (!SSL_set_tlsext_host_name(ssl, ws_host_name.data()))
        {
                std::cerr << "Failed to set the SNI hostname\n";
                exit(EXIT_FAILURE);
        }

        if (!SSL_set1_host(ssl, ws_host_name.data()))
        {
                std::cerr << "Failed to set the certificate verification hostname\n";
                exit(EXIT_FAILURE);
        }
        int ret_ssl_connect = -1;
        while((ret_ssl_connect = SSL_connect(ssl)) != 1)
        {
                if (handle_io_errors(ssl, ret_ssl_connect) == SSL_ERROR::TAME) continue;
                std::cerr << "Failed to connect to server\n";
                exit(EXIT_FAILURE);
        }
        
        // Send Upgrade Request Header
        unsigned char rand_bytes[16];
        RAND_bytes(rand_bytes, sizeof(rand_bytes));
        unsigned char encoded[25];
        EVP_EncodeBlock(encoded, rand_bytes, 16);
        int needed = std::snprintf(nullptr, 0, "GET /?v=10&encoding=json HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n", ws_host_name.data(), encoded);
        std::string s(needed, '\0');
        std::snprintf(s.data(), needed + 1, "GET /?v=10&encoding=json HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\n\r\n", ws_host_name.data(), encoded);

        size_t written_bytes = 0;
        while(!SSL_write_ex(ssl, s.data(), strlen(s.data()), &written_bytes))
        {
                if (handle_io_errors(ssl, 0) == SSL_ERROR::TAME)
                        continue;
                std::cerr << "Failed to write HTTP Request\n";
                exit(EXIT_FAILURE);
        }

        // READ
        int eof = 0;
        char buf[512];
        size_t read_bytes;
        std::string str;
        std::string data_frame;

        std::string frame;
        bool PAYLOAD_FOUND = false;
        unsigned long payload_length = 0;
        while (!eof)
        {
                while (!eof && !SSL_read_ex(ssl, buf, sizeof(buf), &read_bytes))
                {
                        switch(handle_io_errors(ssl, 0))
                        {
                                case SSL_ERROR::TAME:
                                        continue;
                                case SSL_ERROR::END_OF_FILE:
                                        eof = 1;
                                        continue;
                                default:
                                        std::cerr << "Failed reading remaining data\n";
                                        exit(EXIT_FAILURE);
                        }
                }
                if (!eof) 
                {
                        str.append(buf, read_bytes);
                        if (PAYLOAD_FOUND == false)
                        {
                                std::string_view sv(str);
                                std::size_t payload_pos = sv.find("\r\n\r\n");
                                if (payload_pos != std::string::npos)
                                {
                                        if ((payload_pos + 4) < sv.length()) 
                                        {
                                                frame.append(sv.substr(payload_pos));
                                        }
                                        PAYLOAD_FOUND = true;
                                }
                        }
                        else 
                        {
                                frame.append(buf, read_bytes);
                                if (payload_length == 0) payload_length = df_get_payload_length(frame);
                                std::string_view sv(frame);
                                std::size_t first_bracket_pos = sv.find_first_of("{");
                                std::size_t last_bracket_pos = sv.find_last_of("}");
                                std::size_t theo_len = last_bracket_pos - first_bracket_pos + 1;        // +1 to account for zero-based indexing
                                if (first_bracket_pos != std::string::npos && last_bracket_pos != std::string::npos && theo_len == payload_length)
                                {
                                        frame = sv.substr(first_bracket_pos, theo_len);
                                        break;
                                }
                        }
                        //Flush buffer
                        memset(buf, 0, sizeof(buf));
                }
        }

        // Print Out Hello Message
        auto j = json::parse(frame);
        std::cout << j.dump(4) << std::endl;
        
        /* Heartbeat FD */
        int heartbeat_fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
        if (heartbeat_fd == -1)
        {
                perror("Heartbeat FD");
                exit(EXIT_FAILURE);
        }
        // Generate Random Jitter Value
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<double> distrib(0,1);
        double jitter = distrib(gen);

        // Geat Heartbeat Interval
        unsigned long heartbeat_interval_ms = j["d"]["heartbeat_interval"];
        unsigned long heartbeat_interval_s = heartbeat_interval_ms / 1000;
        
        struct itimerspec heartbeat_interval = {};
        heartbeat_interval.it_interval.tv_sec = heartbeat_interval_s;
        heartbeat_interval.it_value.tv_sec = heartbeat_interval_s * jitter;

        int heartbeat_fd_settime_status = timerfd_settime(heartbeat_fd, 0, &heartbeat_interval, NULL);
        if (heartbeat_fd_settime_status == -1)
        {
                perror("timerfd_settime() Heartbeat Error");
                exit(EXIT_FAILURE);
        }

        /* EPOLL */
        // Edge Triggered
        int epoll_fd = epoll_create1(0);
        if (epoll_fd == -1)
        {
                std::cerr << "Failed to create epoll file descriptor\n";
                exit(EXIT_FAILURE);
        }

        // Configure Heartbeat Event 
        struct epoll_event heartbeat_event, events[10];
        heartbeat_event.events = EPOLLIN;
        heartbeat_event.data.fd = heartbeat_fd;
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, heartbeat_fd, &heartbeat_event);

        // Configure WS Event(s)
        struct epoll_event ws_event;
        ws_event.events = EPOLLIN | EPOLLOUT;
        ws_event.data.fd = SSL_get_fd(ssl);
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, SSL_get_fd(ssl), &ws_event);

        // Configure Logger
        this->log_instance.init_event_fd();
        struct epoll_event log_event;
        log_event.events = EPOLLIN;
        log_event.data.fd = this->log_instance.get_event_fd();
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, this->log_instance.get_event_fd(), &log_event);
        
        /* Main Read Loop */
        // This loop has SO many nests it's kind of ugly
        int nfds = -1;
        bool first_heartbeat_sent = false;
        bool heartbeat_ack_recieved = false;
        bool identify_frame_sent = false;
        long long sequence_number = 0;
        while (true)
        {
                nfds = epoll_wait(epoll_fd, events, 10, -1);
                if (nfds == -1)
                {
                        perror("epoll_wait");
                        exit(EXIT_FAILURE);
                }

                for (int i = 0; i < nfds; ++i)
                {
                        if (events[i].data.fd == SSL_get_fd(ssl) && events[i].events & EPOLLOUT && identify_frame_sent == false)
                        {
                                // Send Identity Frame  
                                identify_frame iframe(true, WS_OPCODE::TXT, DC_OPCODE::IDENTIFY, this->bot_token, this->intents);
                                std::size_t str_frame_sz = iframe.build_frame();
                                std::string str_frame = iframe.get_frame();
                                auto bytes_written = std::size_t{};
                                auto a = std::string{iframe.get_raw_payload()};
                                std::size_t write_status = SSL_write_ex(ssl, str_frame.data(), str_frame_sz, &bytes_written);
                                if (write_status < 0) std::cerr << "Error: Could not write correct number of bytes.\n"; 
                                identify_frame_sent = true;
                        }
                        if (events[i].data.fd == heartbeat_fd)
                        {
                                // Might have to double loop to check if the socket is writable
                                uint64_t temp_buf;
                                int read_status = read(heartbeat_fd, &temp_buf, sizeof(temp_buf));
                                if (read_status != -1)
                                {
                                        if (!first_heartbeat_sent)
                                        {
                                                // Send First Heartbeat Here
                                                heartbeat_frame fr(1, WS_OPCODE::TXT, DC_OPCODE::HEARTBEAT, sequence_number);
                                                for (int j = 0; j < nfds && j != i; ++j)
                                                {
                                                        if (events[j].data.fd == SSL_get_fd(ssl) && events[j].events & EPOLLOUT)
                                                        {
                                                                // Socket Ready for Write
                                                                std::size_t built_frame_sz = fr.build_frame();
                                                                std::string built_frame = fr.get_frame();
                                                                std::string testy = fr.get_payload();
                                                                std::cout << testy << "\n";
                                                                std::size_t bytes_written = 0;
                                                                std::size_t write_status = SSL_write_ex(ssl, built_frame.data(), built_frame_sz, &bytes_written);
                                                                if (write_status < 0) std::cerr << "Error: Could not write correct number of bytes.\n"; 
                                                        }
                                                }
                                                first_heartbeat_sent = true;
                                        }
                                }
                        }
                        if (events[i].data.fd == SSL_get_fd(ssl) && events[i].events & EPOLLIN)
                        {
                                char buf_r[4096];
                                size_t read_bytes = 0;
                                while(SSL_read_ex(ssl, buf_r, sizeof(buf_r), &read_bytes))
                                {
                                        this->log_instance.log(buf_r, LOG_LEVEL::INFO);
                                }

                                //Flush Buffer
                                memset(buf, 0, sizeof(buf));
                        }
                        if (events[i].data.fd == this->log_instance.get_event_fd())
                        {
                                this->log_instance.log_all_queue();
                        }
                }
        }

        // Shutdown
        int ret_shutdown = -1;
        while ((ret_shutdown = SSL_shutdown(ssl)) != 1)
        {
                if (ret_shutdown < 0 && handle_io_errors(ssl, ret_shutdown) == SSL_ERROR::TAME) continue;
                std::cerr << "Fatal error while shutting down\n";
                exit(EXIT_FAILURE);
        }
        SSL_free(ssl);
        SSL_CTX_free(ctx);
}

void soul::wait_for_select_read_write(SSL *ssl, bool write)
{
        fd_set fds;
        int width, sock;
        sock = SSL_get_fd(ssl);
        width = sock + 1;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        if (write) select(width, NULL, &fds, NULL, NULL);
        else select(width, &fds, NULL, NULL, NULL);
}

SSL_ERROR soul::handle_io_errors(SSL *ssl, int return_value)
{
        switch(SSL_get_error(ssl, return_value))
        {
                case SSL_ERROR_WANT_READ:
                        wait_for_select_read_write(ssl, false);
                        return SSL_ERROR::TAME;
                case SSL_ERROR_WANT_WRITE:
                        wait_for_select_read_write(ssl, true);
                        return SSL_ERROR::TAME;
                case SSL_ERROR_ZERO_RETURN:
                        return SSL_ERROR::END_OF_FILE;
                case SSL_ERROR_SYSCALL:
                        return SSL_ERROR::FATAL;
                case SSL_ERROR_SSL:
                        if (SSL_get_verify_result(ssl) != X509_V_OK)
                        {
                                std::cout << X509_verify_cert_error_string(SSL_get_verify_result(ssl));
                        }
                        return SSL_ERROR::FATAL;
                default:
                        return SSL_ERROR::FATAL;
        }
}
