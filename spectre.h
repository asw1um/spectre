#ifndef SPECTRE_H
#define SPECTRE_H

#include <iostream>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <errno.h>
#include <stdio.h>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <nlohmann/json.hpp>

namespace spctr
{
        enum class SSL_ERROR
        {
                FATAL,
                END_OF_FILE,
                TAME,
        };

        // Util functions
        std::size_t get_content_length(std::string_view http_response);
        std::string get_payload_str(std::string_view http_response);
        void extract_host_name(std::string &url);

        // Bot class
        class soul
        {       
                private:
                        std::string ws_url;
                        void set_ws_url();
                        void wait_for_select_read_write(SSL *ssl, bool write);
                        spctr::SSL_ERROR handle_io_errors(SSL *ssl, int return_value);

                public:
                        soul();
                        void form();
                        ~soul();
        };
}

// Will be useful for logger later
namespace color 
{
        constexpr const char* reset   = "\033[0m";
        constexpr const char* red     = "\033[31m";
        constexpr const char* green   = "\033[32m";
        constexpr const char* yellow  = "\033[33m";
        constexpr const char* blue    = "\033[34m";
        constexpr const char* bold    = "\033[1m";
}

#endif // !SPECTRE_H
