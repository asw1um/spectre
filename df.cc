#include "spectre.h"

using namespace spctr;

// SAFETY IMPLEMENTATIONS
// p[2] must exist and mask bit must be 0
// These are assumed currently but checks should be done for safety!
// return std::optional?
unsigned long spctr::df_get_payload_length(std::string &payload_buf)
{
        char *p;
        p = payload_buf.data();
        std::bitset<8> byte; 
        for(int i = 0; i <= 7; ++i)
        {
                byte[i] = ( (p[1] >> i) & 1 );
        }
        unsigned long t = byte.to_ulong();
        switch(t)
        {
                case 126:
                        {
                                std::bitset<8> temp_one;
                                std::bitset<8> temp_two;
                                for(int i = 0; i <= 7; ++i)
                                {
                                        temp_one[i] = ( (p[2] >> i) & 1 );
                                }
                                for(int i = 0; i <= 7; ++i)
                                {
                                        temp_two[i] = ( (p[3] >> i) & 1 );
                                }
                                std::string s = temp_one.to_string() + temp_two.to_string();
                                std::bitset<16> f(s);
                                t = f.to_ulong();
                                return t;
                        }
                case 127:
                        {
                                std::bitset<64> f;
                                for (int i = 0; i <= 7; ++i)
                                {
                                        for (int j = 0; j <= 7; ++j)
                                        {
                                                f[j + 8*i] = ((p[i+2] >> j)&1);        
                                        }
                                }
                                t = f.to_ulong();
                                return t;
                        }
                default:
                        return t;
        }
}

data_frame::data_frame()
{
        std::cout << "Data Frame Constructor\n";
}

data_frame::~data_frame()
{
        std::cout << "Data Frame Destructor\n";
}
