#include <iostream>
#include <string>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <cstring>

#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <nlohmann/json.hpp>

const char *HOST_NAME   = "discord.com";

using SOCKET = int;
using json = nlohmann::json;

namespace color {
    constexpr const char* reset   = "\033[0m";
    constexpr const char* red     = "\033[31m";
    constexpr const char* green   = "\033[32m";
    constexpr const char* yellow  = "\033[33m";
    constexpr const char* blue    = "\033[34m";
    constexpr const char* bold    = "\033[1m";
}

// Remember to free relevant items when done;
// addrinfo struct (linked list) needs to be freed after address is obtained

bool verify_content_type_json();
std::size_t get_content_length(std::string_view http_response);
std::string get_payload_str(std::string_view http_response);
void boot();

int main()
{
        boot();               
}

void boot()
{
        SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
        if (ctx == NULL)
        {
                std::cout << "Failed to create the SSL_CTX\n";
                exit(EXIT_FAILURE);
        }

        SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

        // READ ABOUT THIS FUNCTION!
        if (!SSL_CTX_set_default_verify_paths(ctx))
        {
                std::cout << "Failed to set the default trusted certificate store\n";
                exit(EXIT_FAILURE);
        }
        
        // endpoint uses latest TLS version already, but just to be doubly sure
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
        // Set struct to be empty
        struct addrinfo hints{};
        hints.ai_family = AF_INET;              // care about IPV4 only
        hints.ai_socktype = SOCK_STREAM;        // socket is TCP
        hints.ai_flags = AI_PASSIVE;
        // memset(&hints, 0, sizeof(hints)); 
        
        struct addrinfo *servinfo;
        getaddrinfo_status = getaddrinfo(HOST_NAME, "https", &hints, &servinfo);
        if (getaddrinfo_status != 0)
        {
                std::cerr << "getaddrinfo: " << gai_strerror(getaddrinfo_status) << "\n";
                exit(EXIT_FAILURE);
        }
        // struct addrinfo {
        //        int              ai_flags;
        //        int              ai_family;
        //        int              ai_socktype;
        //        int              ai_protocol;
        //        socklen_t        ai_addrlen;
        //        struct sockaddr *ai_addr;
        //        char            *ai_canonname;
        //        struct addrinfo *ai_next;
        //    };
        // struct sockaddr_in {
        //         sa_family_t     sin_family;     /* AF_INET */
        //         in_port_t       sin_port;       /* Port number */
        //         struct in_addr  sin_addr;       /* IPv4 address */
        // };

        // char ipstr[INET6_ADDRSTRLEN];
        SOCKET main_sock = -1;
        for (struct addrinfo *p = servinfo; p != NULL; p = p->ai_next)
        {
                using namespace std;
                // BLOCKING socket currently
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
                // PRINT ALL IP ADDRESSES
                // void *addr;
                // struct sockaddr_in *t = (struct sockaddr_in *) p->ai_addr;
                // addr = &(t->sin_addr);
                // inet_ntop(p->ai_family, addr, ipstr, sizeof(ipstr));
                // cout << ipstr << "\n";
        } 

        BIO *bio;
        bio = BIO_new(BIO_s_socket());
        if (bio == NULL)
        {
                BIO_closesocket(main_sock);
                exit(EXIT_FAILURE);
        }
        
        // BIO_CLOSE ensures that when BIO is free'd the socket is free'd as well
        BIO_set_fd(bio, main_sock, BIO_CLOSE);

        // SSL is now responsible for freeing BIO and by extension socket
        SSL_set_bio(ssl, bio, bio);

        if (!SSL_set_tlsext_host_name(ssl, HOST_NAME))
        {
                std::cerr << "Failed to set the SNI hostname\n";
                exit(EXIT_FAILURE);
        }

        if (!SSL_set1_host(ssl, HOST_NAME))
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

        // Sending and Receiving Data
        size_t written;
        
        // Connection: close since we only care for the ws url the server sends
        const char *req = "GET /api/v10/gateway HTTP/1.1\r\nHost: discord.com\r\nUser-Agent: SPECTRE/1.0.0\r\nAccept: */*\r\nConnection: close\r\n\r\n\r\n";
        if (!SSL_write_ex(ssl, req, strlen(req), &written))
        {
                std::cerr << "Failed to write HTTP request\n";
                exit(EXIT_FAILURE);
        }
        
        size_t read_bytes;
        char buf[256];
        std::string str;
        while(SSL_read_ex(ssl, buf, sizeof(buf), &read_bytes)) str.append(buf, read_bytes);
        
        std::cout << "Recieved JSON Payload is: \n"; 
        std::cout << get_payload_str(str);
        
        if (SSL_get_error(ssl, 0) != SSL_ERROR_ZERO_RETURN)
        {
                std::cerr << "Failed reading remaining data\n";
                exit(EXIT_FAILURE);
        }

        // Shutdown 
        int ret = SSL_shutdown(ssl);
        if (ret < 1)
        {
                std::cerr << "Error shutting down\n";
                exit(EXIT_FAILURE);
        }
        SSL_free(ssl);
        SSL_CTX_free(ctx);
}

std::size_t get_content_length(std::string_view http_response) 
{
        std::size_t pos = http_response.find_first_of("Content-Length: ");
        std::size_t bpos = http_response.find_first_of(" ", pos) + 1;
        std::size_t epos = http_response.find_first_of("\n", bpos);
        std::size_t len = epos - bpos;
        std::string str(http_response.substr(bpos, len));
        return (std::size_t) std::stoull(str, nullptr);
}

std::string get_payload_str(std::string_view http_response)
{
        std::size_t payload_len = get_content_length(http_response);
        std::size_t epos = http_response.find_last_of("}") + 1;
        std::size_t bpos = http_response.find_first_of("{", epos - payload_len);
        std::string ret(http_response.substr(bpos, payload_len));
        return ret;
}
