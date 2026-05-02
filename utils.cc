#include "spectre.h"

using namespace spctr;

void spctr::extract_host_name(std::string &url)
{
        std::size_t bpos = url.find_first_of("://") + 3;
        std::size_t len = url.find("?", bpos) - 1 - bpos;
        url = url.substr(bpos, len);
}

std::size_t spctr::get_content_length(std::string_view http_response) 
{
        std::size_t pos = http_response.find_first_of("Content-Length: ");
        std::size_t bpos = http_response.find_first_of(" ", pos) + 1;
        std::size_t epos = http_response.find_first_of("\n", bpos);
        std::size_t len = epos - bpos;
        std::string str(http_response.substr(bpos, len));
        return (std::size_t) std::stoull(str, nullptr);
}
