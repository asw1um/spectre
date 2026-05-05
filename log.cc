#include "spectre.h"

using namespace spctr;

namespace 
{
        constexpr const char* reset   = "\033[0m";
        constexpr const char* red     = "\033[31m";
        constexpr const char* green   = "\033[32m";
        constexpr const char* yellow  = "\033[33m";
        constexpr const char* blue    = "\033[34m";
        constexpr const char* bold    = "\033[1m";
}
/* Logger Instance is created on startup and setup on ready event
 * Custom Logger can be initiated by user 
 * Allows for custom logs, aside from framework provided/sent logs 
 */
logger::logger()
{
         
}

void logger::init_event_fd()
{
        // Init With Initial Value so we empty queue upon ready
        this->event_fd = eventfd(1, EFD_NONBLOCK);
        if (this->event_fd == -1) 
        {
                perror("eventfd()");
                exit(EXIT_FAILURE);
        }
        this->deferred = false;
}

logger::~logger()
{
        close(this->event_fd);
}

int logger::get_event_fd()
{
        return this->event_fd;
}

std::string logger::get_time()
{
        time_t res;
        res = time(NULL);
        char *f = asctime(localtime(&res)); 
        std::string h{f};
        h.pop_back();
        return h;
}

void logger::log(std::string_view output, LOG_LEVEL log_level)
{
        struct log_msg tlog{output, log_level};
        this->log_buf.push(tlog);
}

void logger::log_all_queue()
{
        if (!this->log_buf.empty())
        {
                // Signal Event FD (Unsafe)
                uint64_t code = 1;
                write(this->event_fd, &code, sizeof(code));
        }
        for (; !this->log_buf.empty(); this->log_buf.pop())
        {
                struct log_msg current_log = this->log_buf.front();
                switch(current_log.log_level)
                {
                        case LOG_LEVEL::DEBUG:
                                std::cout << "[" << this->get_time() << "] "<< "[" << blue << "DEBUG" << reset << "] : " << current_log.msg << "\n"; 
                                break;
                        case LOG_LEVEL::INFO:
                                std::cout << "[" << this->get_time() << "] "<< "[" << "INFO" << "] : " << current_log.msg << "\n"; 
                                break;
                        case LOG_LEVEL::SUCCESS:
                                std::cout << "[" << this->get_time() << "] "<< "[" << green << "SUCCESS" << reset << "] : " << current_log.msg << "\n"; 
                                break;
                        case LOG_LEVEL::WARNING:
                                std::cout << "[" << this->get_time() << "] "<< "[" << yellow << "WARNING" << reset << "] : " << current_log.msg << "\n"; 
                                break;
                        case LOG_LEVEL::ERROR:
                                std::cout << "[" << this->get_time() << "] "<< "[" << red << "ERROR" << reset << "] : " << current_log.msg << "\n"; 
                                break;
                        case LOG_LEVEL::CRITICAL:
                                std::cout << "[" << this->get_time() << "] "<< "[" << red << bold << "CRITICAL" << reset << "] : " << current_log.msg << "\n"; 
                                break;
                }
        }
}
