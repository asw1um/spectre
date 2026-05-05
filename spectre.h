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
#include <time.h>
#include <bitset>
#include <random>
#include <netdb.h>
#include <queue>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
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
        /* Util Functions */
        std::size_t get_content_length(std::string_view http_response);
        std::string get_payload_str(std::string_view http_response);
        void extract_host_name(std::string &url);
        unsigned long df_get_payload_length(std::string &payload);
 
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
                HEARTBEAT = 0x1,
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

        enum class LOG_LEVEL
        {
                DEBUG,
                INFO,
                SUCCESS,
                WARNING,
                ERROR,
                CRITICAL
        };
        
        /* Only USER should be accessible, others are used within spectre code */
        enum class LOG_SUPPLIER
        {
                SYS,
                DISCORD,
                SPECTRE,
                USER
        };
       
        // Class is abstract due to existence of pure virtual functions
        class data_frame
        {
                protected:
                        bool fin;
                        WS_OPCODE ws_opcode;
                        DC_OPCODE dc_opcode; 
                        bool masked;
                        uint32_t masking_key;
                        std::size_t payload_length;
                        std::string payload;    /* Payload stored in masked form */
                        std::string cached_frame; /* Everytime build_frame is called frame should be cached */

                        uint32_t generate_masking_key();
                        void mask_payload(uint32_t &masking_key, std::string &i_payload);
                        SPCTR_ERROR validate_payload();
                public:
                        data_frame(bool i_fin, WS_OPCODE i_ws_opcode, DC_OPCODE dc_opcode, bool i_masked);
                        
                        virtual std::string construct_payload() = 0;
                        virtual std::string build_frame() = 0;
        };

        class heartbeat_frame : data_frame
        {
                unsigned long int seq_num;
                public:
                        heartbeat_frame(bool i_fin, WS_OPCODE i_ws_opcode, DC_OPCODE i_dc_opcode, unsigned long int i_seq_num);
                        std::string construct_payload();
                        std::string build_frame();
                        void print_unmasked_payload(std::string copy_frame);
                        void set_seq_num(unsigned long int i_seq_num)
                        {
                                this->seq_num = i_seq_num;
                        }
                        std::string get_payload()
                        {
                                std::string copy = this->payload;
                                this->mask_payload(this->masking_key, copy);
                                return copy;
                        }
        };

        struct log_msg 
        {
                std::string_view msg;
                LOG_LEVEL log_level;
        };

        class logger
        {
                int event_fd;
                bool deferred = true;
                std::queue<log_msg> log_buf;
                std::string get_time();
                public:
                        logger();
                        ~logger();
                        int get_event_fd();
                        void init_event_fd();
                        void log(std::string_view output, LOG_LEVEL log_level);
                        void log_all_queue();
        };

        class soul
        {       
                std::string ws_url;
                void wait_for_select_read_write(SSL *ssl, bool write);
                SSL_ERROR handle_io_errors(SSL *ssl, int return_value);
                logger log_instance;

                public:
                        soul();
                        ~soul();
                        void form();
                        void log(std::string_view output, LOG_LEVEL log_level);
        };
}


#endif // !SPECTRE_H
