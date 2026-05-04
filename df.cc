#include "spectre.h"

using namespace spctr;

// TODO: SAFETY IMPLEMENTATIONS
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


data_frame::data_frame(bool i_fin, WS_OPCODE i_opcode, bool i_masked)
{
        this->fin = i_fin;
        this->opcode = i_opcode;
        this->masked = i_masked;
        std::cout << "Data Frame Constructor\n";
}

data_frame::~data_frame()
{
        std::cout << "Data Frame Destructor\n";
}

uint32_t data_frame::generate_masking_key()
{
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> distrib(std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());
        return (uint32_t) distrib(gen);
}

void data_frame::mask_payload(uint32_t &masking_key, std::size_t &payload_length, std::string &payload)
{
        std::bitset<32> key(masking_key);                 
        for(std::size_t i = 0; i < payload_length; ++i)
        {
                int j = i % 4;
                payload[i] = payload[i] ^ key[j];
        }
}

SPCTR_ERROR data_frame::validate_payload(std::string_view payload)
{
        std::size_t len = payload.length();
        if (len < 4096) return SPCTR_ERROR::PAYLOAD_OK;
        else return SPCTR_ERROR::PAYLOAD_TOO_LONG;
}

heartbeat_frame::heartbeat_frame(bool i_fin, WS_OPCODE i_opcode, std::size_t i_payload_length, std::string i_payload) : data_frame(i_fin, i_opcode, true)
{
        this->masking_key = this->generate_masking_key();
        SPCTR_ERROR valid = validate_payload(i_payload);
        if (valid == SPCTR_ERROR::PAYLOAD_TOO_LONG) 
        {
                std::cerr << "Supplied Payload is Too Long";
                exit(EXIT_FAILURE);
        }
        this->payload_length = i_payload_length;
        this->mask_payload(this->masking_key, i_payload_length, i_payload);
}

std::string heartbeat_frame::build_frame()
{
        std::bitset<8> first_byte;
        first_byte[0] = this->fin;
        first_byte[1] = 0;
        first_byte[2] = 0;
        first_byte[3] = 0;
        int opcode = static_cast<int>(this->opcode);
        std::bitset<4> opcode_bits(opcode);
        
        // Discord rejects all payloads greater than 4096 bytes
        for (int i = 4; i < 8; ++i) first_byte[i] = opcode_bits[i-4];

        char mask_bit = static_cast<char>(this->masked);
        if (this->payload_length < 126)
        {
                std::bitset<7> payload_len(this->payload_length);
                std::bitset<32> masking_key_bits(this->masking_key);

                // Serialize Masked Data
                std::size_t num_bits = this->payload_length * 8;
                std::string final_frame = first_byte.to_string();
                final_frame.append(opcode_bits.to_string(), 4);
                final_frame.append(1, mask_bit);
                final_frame.append(payload_len.to_string(), 7);
                final_frame.append(masking_key_bits.to_string(), 32);
                final_frame.append(this->payload);
                this->cached_frame = final_frame;
                return final_frame;
        }
        std::bitset<7> payload_len(126);
        std::bitset<16> ex_payload_len(this->payload_length);
        std::bitset<32> masking_key_bits(this->masking_key);

        // Serialize Masked Data
        std::string final_frame = first_byte.to_string();
        final_frame.append(opcode_bits.to_string(), 4);
        final_frame.append(1, mask_bit);
        final_frame.append(payload_len.to_string(), 7);
        final_frame.append(ex_payload_len.to_string(), 16);
        final_frame.append(masking_key_bits.to_string(), 32);
        final_frame.append(this->payload);
        this->cached_frame = final_frame;
        return final_frame;
}

// std::string_view ro_build_frame()
// {
//
// }
