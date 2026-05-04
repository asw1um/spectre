#ifndef SPECTRE_H
#define SPECTRE_H

#include <iostream>
#include <string>
#include <unistd.h>
#include <stdint.h>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <errno.h>
#include <stdio.h>
#include <bitset>
#include <random>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/timerfd.h>
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
        
        enum class DC_OPCODE
        {
                // opcodes shared within Discord's JSON Payload
                DISPATCH,
                HEARTBEAT,
                IDENTIFY,
                VOICE_STATE_UPDATE = 4,
                RESUME = 6,
                RECONNECT,
                REQUEST_GUILD_MEMBERS,
                INVALID_SESSION,
                HELLO,
                HEARTBEAT_ACK,
                REQUEST_CHANNEL_INFO = 43
        };

        enum class WS_OPCODE
        {
                // 0x3 - 0x7 Reserved for non-control frames
                // 0xB - 0xF Reserved for further control frames
                // Currently may help with frame context chcking
                CONT = 0x0,
                TXT = 0x1,
                BIN = 0x2,
                CONN_CLOSE = 0x8,
                PING = 0x9,
                PONG = 0xA,
        };
        
        enum class SPCTR_ERROR
        {
                PAYLOAD_OK,
                PAYLOAD_TOO_LONG,
        };

        std::size_t get_content_length(std::string_view http_response);
        std::string get_payload_str(std::string_view http_response);
        void extract_host_name(std::string &url);
        unsigned long df_get_payload_length(std::string &payload);
        
        // Class is abstract due to existence of pure virtual functions
        class data_frame
        {
                protected:
                        bool fin;
                        WS_OPCODE opcode;
                        bool masked;
                        uint32_t masking_key;
                        std::size_t payload_length;
                        std::string payload;    /* Payload stored in masked form */
                        std::string cached_frame; /* Everytime build_frame is called frame should be cached */

                        uint32_t generate_masking_key();
                        void mask_payload(uint32_t &masking_key, std::size_t &payload_length, std::string &payload);
                        SPCTR_ERROR validate_payload(std::string_view payload);
                public:
                        data_frame(bool i_fin, WS_OPCODE i_opcode, bool i_masked);
                        WS_OPCODE get_opcode() { return this->opcode; }
                        ~data_frame();
                        
                        virtual std::string build_frame() = 0;
                        // virtual std::string_view ro_build_frame() = 0; /* Get's a read only copy of the frame*/ 
        };

        class heartbeat_frame : data_frame
        {
                public:
                        heartbeat_frame(bool i_fin, WS_OPCODE i_opcode, std::size_t i_payload_length, std::string i_payload);
                        std::string build_frame();

                        std::string_view get_payload_sv()
                        {
                                std::string_view sv(this->payload);
                                return sv;
                        }
        };

        class soul
        {       
                std::string ws_url;
                void wait_for_select_read_write(SSL *ssl, bool write);
                SSL_ERROR handle_io_errors(SSL *ssl, int return_value);

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
