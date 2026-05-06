#include "spectre.h"
#include <cstdint>

using namespace spctr;

template <std::size_t N>
void flip_bitset(std::bitset<N> &bs)
{
        std::size_t start = 0;
        std::size_t end = N - 1;
        while (start < end)
        {
                int temp = bs[start];
                bs[start] = bs[end];
                bs[end] = temp;
                start++;
                end--;
        }
}

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


data_frame::data_frame(bool i_fin, WS_OPCODE i_ws_opcode, DC_OPCODE i_dc_opcode, bool i_masked)
{
        this->fin = i_fin;
        this->ws_opcode = i_ws_opcode;
        this->dc_opcode = i_dc_opcode;
        this->masked = i_masked;
}

uint32_t data_frame::generate_masking_key()
{
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint32_t> distrib(std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());
        return (uint32_t) distrib(gen);
}

void data_frame::mask_payload(uint32_t &masking_key, std::string &i_payload)
{
        std::bitset<32> key(masking_key);                 
        flip_bitset(key);
        std::size_t payload_length = i_payload.length();
        std::string transformed_data;
        for(std::size_t i = 0; i < payload_length; ++i)
        {
                int j = i % 4;
                std::bitset<8> octet;
                for (int k = 0; k < 8; ++k)
                {
                        octet[k] = key[k + 8*j];
                }
                unsigned long octet_val = octet.to_ulong();
                unsigned char octet_char = static_cast<char>(octet_val);
                i_payload[i] = i_payload[i] ^ octet_char;
        }
        this->payload = i_payload;
}

SPCTR_ERROR data_frame::validate_payload()
{
        std::size_t len = this->payload.length();
        if (len < 4096) return SPCTR_ERROR::PAYLOAD_OK;
        else return SPCTR_ERROR::PAYLOAD_TOO_LONG;
}

heartbeat_frame::heartbeat_frame(bool i_fin, WS_OPCODE i_ws_opcode, DC_OPCODE i_dc_opcode, unsigned long int i_seq_num) : data_frame(i_fin, i_ws_opcode, i_dc_opcode, true)
{
        this->masking_key = this->generate_masking_key();
        this->seq_num = i_seq_num;
        this->payload = this->construct_payload();
        SPCTR_ERROR valid = validate_payload();
        if (valid == SPCTR_ERROR::PAYLOAD_TOO_LONG) 
        {
                std::cerr << "Supplied Payload is Too Long";
                exit(EXIT_FAILURE);
        }
        this->payload_length = this->payload.length();
        this->mask_payload(this->masking_key, this->payload);
}

/* 
 * It's likely that this function is incomplete, since the larger payload case hasn't been tested
 * There's some strange behaviour occuring when doing operations using bitset, regarding the ordering
 * It is imperative that the bits be ordered in Big Endian format, so they must be flipped/unflipped accordingly
 * bitset stores them in little endian ordering 
 *
 * Perhaps a wrapped class would solve this
 */
std::size_t heartbeat_frame::build_frame()
{
        std::bitset<8> first_byte;
        first_byte[0] = this->fin;
        first_byte[1] = 0;
        first_byte[2] = 0;
        first_byte[3] = 0;
        int opcode = static_cast<int>(this->ws_opcode);
        std::bitset<4> opcode_bits(opcode);
        flip_bitset(opcode_bits);
        
        // Discord rejects all payloads greater than 4096 bytes
        for (int i = 4; i < 8; ++i) first_byte[i] = opcode_bits[i-4];
        unsigned char first_char = static_cast<char>(first_byte.to_ulong());

        if (this->payload_length < 126)
        {
                std::bitset<8> second_byte;
                second_byte[0] = 1;
                std::bitset<7> payload_len(this->payload_length);
                flip_bitset(payload_len);
                for (int i = 1; i < 8; ++i) second_byte[i] = payload_len[i-1];
                std::bitset<32> masking_key_bits(this->masking_key);
                flip_bitset(masking_key_bits);
                
                flip_bitset(second_byte);
                unsigned char second_char = static_cast<char>(second_byte.to_ulong());

                std::string masking_key_chars;
                for (int i = 0; i < 32; i+=8)
                {
                        std::bitset<8> tbs; 
                        for (int j = 0; j < 8; ++j)
                        {
                                tbs[j] = masking_key_bits[j + i]; 
                        }
                        masking_key_chars.append(1, static_cast<char>(tbs.to_ulong()));
                }
                // Serialize Masked Data
                std::string final_frame;
                final_frame.append(1, first_char);
                final_frame.append(1, second_char);
                final_frame.append(masking_key_chars);
                final_frame.append(this->payload);
                this->cached_frame = final_frame;
                return final_frame.size();
        }

        // Serialize Masked Data
        std::bitset<7> payload_len(126);
        std::bitset<16> ex_payload_len(this->payload_length);
        std::bitset<32> masking_key_bits(this->masking_key);
        flip_bitset(payload_len);
        flip_bitset(ex_payload_len);
        flip_bitset(masking_key_bits);

        std::bitset<8> second_byte;
        second_byte[0] = this->masked;
        for (int i = 1; i < 8; ++i) second_byte[i] = payload_len[i-1];
        flip_bitset(second_byte);

        unsigned char second_char = static_cast<char>(second_byte.to_ulong());

        std::string ex_payload_len_chars;
        for (int i = 0; i < 2; i+=2)
        {
                std::bitset<8> tbs;
                for (int j = 0; j < 8; ++j)
                {
                        tbs[j] = ex_payload_len[j+i];
                }
                ex_payload_len_chars.append(1, static_cast<char>(tbs.to_ulong()));
        }

        std::string masking_key_chars;
        for (int i = 0; i < 32; i+=8)
        {
                std::bitset<8> tbs; 
                for (int j = 0; j < 8; ++j)
                {
                        tbs[j] = masking_key_bits[j + i]; 
                }
                masking_key_chars.append(1, static_cast<char>(tbs.to_ulong()));
        }

        std::string final_frame;
        final_frame.append(1, first_char);
        final_frame.append(1, second_char);
        final_frame.append(ex_payload_len_chars);
        final_frame.append(masking_key_chars);
        final_frame.append(this->payload);
        this->cached_frame = final_frame;
        return final_frame.size();
}

/* 
 * The definition of this function is rather dangerous since we don't provide any default values in the class definition
 * We are forced to remember to call this function last 
 * I.E This should change
 */
std::string heartbeat_frame::construct_payload()
{
        using ordered_json = nlohmann::ordered_json;
        using std::to_string;   
        if (this->seq_num == 0) 
        {
                ordered_json j;
                j["op"] = DC_OPCODE::HEARTBEAT;
                j["d"] = nlohmann::json::value_t::null;
                auto jstr = to_string(j);
                return jstr;
        }
        ordered_json j;
        j["op"] = DC_OPCODE::HEARTBEAT;
        j["d"] = this->seq_num;
        auto jstr = to_string(j);
        return jstr;
}

inline void heartbeat_frame::print_unmasked_payload(std::string copy_frame)
{
        this->mask_payload(this->masking_key, copy_frame);
        std::cout << copy_frame << "\n";
}


identify_frame::identify_frame(bool i_fin, WS_OPCODE i_ws_opcode, DC_OPCODE i_dc_opcode, std::string_view i_bot_token, INTENTS &i_intents) : data_frame(i_fin, i_ws_opcode, i_dc_opcode, true)
{
        this->intents = i_intents;
        this->bot_token = i_bot_token;
        this->masking_key = this->generate_masking_key();
        this->payload = this->construct_payload();
        SPCTR_ERROR valid = validate_payload();
        if (valid == SPCTR_ERROR::PAYLOAD_TOO_LONG) 
        {
                std::cerr << "Supplied Payload is Too Long";
                exit(EXIT_FAILURE);
        }
        this->payload_length = this->payload.length();
        this->mask_payload(this->masking_key, this->payload);
}

std::string identify_frame::construct_payload()
{
        using ordered_json = nlohmann::ordered_json;
        using std::to_string;   
        ordered_json j;
        j["op"] = DC_OPCODE::IDENTIFY;
        j["d"]["token"] = this->bot_token;
        j["d"]["properties"]["os"] = "linux";
        j["d"]["properties"]["browser"] = "spectre";
        j["d"]["properties"]["device"] = "spectre";
        j["d"]["compress"] = false;
        j["d"]["shard"] = nlohmann::json::array({0,1});
        j["d"]["intents"] = static_cast<uint16_t>(this->intents);
        auto jstr = to_string(j);
        return jstr;
}

std::size_t identify_frame::build_frame()
{
        std::bitset<8> first_byte;
        first_byte[0] = this->fin;
        first_byte[1] = 0;
        first_byte[2] = 0;
        first_byte[3] = 0;
        int opcode = static_cast<int>(this->ws_opcode);
        std::bitset<4> opcode_bits(opcode);
        flip_bitset(opcode_bits);

        // Discord rejects all payloads greater than 4096 bytes
        for (int i = 4; i < 8; ++i) first_byte[i] = opcode_bits[i-4];
        unsigned char first_char = static_cast<char>(first_byte.to_ulong());

        if (this->payload_length < 126)
        {
                std::bitset<8> second_byte;
                second_byte[0] = 1;
                std::bitset<7> payload_len(this->payload_length);
                flip_bitset(payload_len);
                for (int i = 1; i < 8; ++i) second_byte[i] = payload_len[i-1];
                std::bitset<32> masking_key_bits(this->masking_key);
                flip_bitset(masking_key_bits);

                flip_bitset(second_byte);
                unsigned char second_char = static_cast<char>(second_byte.to_ulong());

                std::string masking_key_chars;
                for (int i = 0; i < 32; i+=8)
                {
                        std::bitset<8> tbs; 
                        for (int j = 0; j < 8; ++j)
                        {
                                tbs[j] = masking_key_bits[j + i]; 
                        }
                        masking_key_chars.append(1, static_cast<char>(tbs.to_ulong()));
                }
                // Serialize Masked Data
                std::string final_frame;
                final_frame.append(1, first_char);
                final_frame.append(1, second_char);
                final_frame.append(masking_key_chars);
                final_frame.append(this->payload);
                this->cached_frame = final_frame;
                // We are guaranteed here that all chars are >0 (ascii value) so we can use default function
                return final_frame.size();
        }

        // Serialize Masked Data
        std::bitset<7> payload_len(126);
        std::bitset<16> ex_payload_len(this->payload_length);
        std::bitset<32> masking_key_bits(this->masking_key);
        flip_bitset(payload_len);
        flip_bitset(ex_payload_len);
        flip_bitset(masking_key_bits);

        std::bitset<8> second_byte;
        second_byte[0] = this->masked;
        for (int i = 1; i < 8; ++i) second_byte[i] = payload_len[i-1];
        flip_bitset(second_byte);
        unsigned char second_char = static_cast<char>(second_byte.to_ulong());

        auto third_byte = std::bitset<8>{};
        for(int i = 0; i < 8; ++i) third_byte[i] = ex_payload_len[i];
        flip_bitset(third_byte);
        unsigned char third_char = static_cast<char>(third_byte.to_ulong());

        auto fourth_byte = std::bitset<8>{};
        for(int i = 0; i < 8; ++i) fourth_byte[i] = ex_payload_len[i+8];
        flip_bitset(fourth_byte);
        unsigned char fourth_char = static_cast<char>(fourth_byte.to_ulong());

        std::string masking_key_chars;
        for (int i = 0; i < 32; i+=8)
        {
                std::bitset<8> tbs; 
                for (int j = 0; j < 8; ++j)
                {
                        tbs[j] = masking_key_bits[j + i]; 
                }
                masking_key_chars.append(1, static_cast<char>(tbs.to_ulong()));
        }

        auto final_frame = std::string{};
        final_frame.append(1, first_char);
        final_frame.append(1, second_char);
        final_frame.append(1, third_char);
        final_frame.append(1, fourth_char);
        final_frame.append(masking_key_chars);
        final_frame.append(this->payload);
        this->cached_frame = final_frame;
        std::size_t final_sz = 4 + masking_key_chars.size() + this->payload.size();
        return final_sz;
}
