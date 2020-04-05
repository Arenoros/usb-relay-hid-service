#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

#include "platform_conf.h"

inline void print_bin(const std::vector<uint8_t>& data) {
    printf("Binary: \n\t");
    for(size_t i = 0; i < data.size(); ++i) {
        printf("0x%02x ", data[i]);
        if(i + 1 % 16 == 0) printf("\n\t");
    }
    printf("\n");
}
inline void print_bin(const uint8_t* data, int size) {
    printf("Binary: \n\t");
    for(int i = 0; i < size; ++i) {
        printf("0x%02x ", data[i]);
        if(i + 1 % 16 == 0) printf("\n\t");
    }
    printf("\n");
}
inline void print_text(const std::string& data) {
    printf("Text: \n");
    printf("\t%s\n", data.c_str());
}
namespace mplc {
    class TcpSocket;

    struct WSFrame {
        enum TOpcode {
            Continue = 0x0,
            Text = 0x1,
            Binary = 0x2,
            Close = 0x8,
            Ping = 0x9,
            Pong = 0x10
        };
        WSFrame(uint8_t* buf, int size);
        WSFrame()
            : fin(0), rsv(0), opcode(0), has_mask(0), short_len(0), _mask(0), head_len(0),
              len(0), payload_len(0), payload(nullptr), _buf(nullptr), _buf_size(0),
              _recv_size(0), frame_end(0) {}

        int map_on(uint8_t* buf, int size);
        void decode();
        void encode();
        int load_from(const TcpSocket& sock);
        int load(int size);
        int send_to(TcpSocket& sock) const;
        bool has_frame() const;
        void next();
        // ws frame headers
        uint8_t fin : 1;
        uint8_t rsv : 3;
        uint8_t opcode : 4;
        uint8_t has_mask : 1;
        uint8_t short_len : 7;
        union {
            uint8_t mask[4];
            uint32_t _mask;
        };
        //-------
        uint8_t head_len;        // size of frame header
        int len;                 // Size of payload data in buffer for current frame
        uint64_t payload_len;    // full size payload data
        //uint64_t payload_begin;            // Position relate of payload data
        const uint8_t* payload;  // pointer to start payload data in buffer for current frame

    private:
        uint8_t* _buf;  // pointer to start buffer
        int _buf_size,
            _recv_size,  // Size of date read from socket
            frame_end;    // Position to end in buffer frame
    };
}  // namespace mplc
