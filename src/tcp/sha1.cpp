/* sha1.cpp

Copyright (c) 2005 Michael D. Leonhard

http://tamale.net/

This file is licensed under the terms described in the
accompanying LICENSE file.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "sha1.h"
#include <cstdint>
#include <Winsock2.h>

namespace mplc {
    void to_base64(const uint8_t* data, size_t data_size, size_t b64_size, char* out) {
        static const char encoding_table[64] = {

            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
            'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
            'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
            'w', 'x', 'y', 'z', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '+', '/'};
        for(size_t i = 0, j = 0; i < data_size;) {
            uint32_t octet_a = data[i++];
            uint32_t octet_b = i < data_size ? data[i++] : 0;
            uint32_t octet_c = i < data_size ? data[i++] : 0;
            uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
            // uint32_t n_long = htonl(triple);
            out[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
            out[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
            out[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
            out[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
        }
        static const int mod_table[] = {0, 2, 1};
        for(int i = 0; i < mod_table[data_size % 3]; i++) out[b64_size - 1 - i] = '=';
    }
    void from_base64(const char* b64, size_t b64_size, size_t data_size, uint8_t* out) {
        static const char decoding_table[128] = {

            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62,
            64, 64, 64, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 0,  64, 64, 64, 0,
            1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
            23, 24, 25, 64, 64, 64, 64, 64, 64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38,
            39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64};

        for(unsigned int i = 0, j = 0; i < b64_size;) {
            uint32_t sextet_a = decoding_table[b64[i++]];
            uint32_t sextet_b = decoding_table[b64[i++]];
            uint32_t sextet_c = decoding_table[b64[i++]];
            uint32_t sextet_d = decoding_table[b64[i++]];

            uint32_t triple = (sextet_a << 3 * 6) + (sextet_b << 2 * 6) + (sextet_c << 1 * 6) +
                              (sextet_d << 0 * 6);
            if(j < data_size) out[j++] = (triple >> 2 * 8) & 0xFF;
            if(j < data_size) out[j++] = (triple >> 1 * 8) & 0xFF;
            if(j < data_size) out[j++] = (triple >> 0 * 8) & 0xFF;
        }
    }
    // print out memory in hexadecimal
    void SHA1::hexPrinter(uint8_t* c, int l) {
        assert(c);
        assert(l > 0);
        while(l > 0) {
            printf(" %02x", *c);
            l--;
            c++;
        }
    }

    // circular left bit rotation.  MSB wraps around to LSB
    uint32_t SHA1::lrot(uint32_t x, int bits) { return (x << bits) | (x >> (32 - bits)); };
    void SHA1::storeBigEndianUint32(uint8_t* byte, uint32_t num) {
        assert(byte);
        byte[0] = (uint8_t)(num >> 24);
        byte[1] = (uint8_t)(num >> 16);
        byte[2] = (uint8_t)(num >> 8);
        byte[3] = (uint8_t)num;
    }
    // Constructor *******************************************************
    SHA1::SHA1(): bytes{} {
        // make sure that the data type is the right size
        assert(sizeof(uint32_t) * 5 == 20);

        // initialize
        Hn[0] = 0x67452301;
        Hn[1] = 0xefcdab89;
        Hn[2] = 0x98badcfe;
        Hn[3] = 0x10325476;
        Hn[4] = 0xc3d2e1f0;
        unprocessedBytes = 0;
        size = 0;
    }

    // Destructor ********************************************************
    SHA1::~SHA1() {
        // erase data
        Hn[0] = Hn[1] = Hn[2] = Hn[3] = Hn[4] = 0;
        for(int c = 0; c < 64; c++) bytes[c] = 0;
        unprocessedBytes = size = 0;
    }

    // process ***********************************************************
    void SHA1::process() {
        assert(unprocessedBytes == 64);
        // printf( "process: " ); hexPrinter( bytes, 64 ); printf( "\n" );
        int t;
        uint32_t a, b, c, d, e, K, f, W[80];
        // starting values
        a = Hn[0];
        b = Hn[1];
        c = Hn[2];
        d = Hn[3];
        e = Hn[4];
        // copy and expand the message block
        for(t = 0; t < 16; t++)
            W[t] = (bytes[t * 4] << 24) + (bytes[t * 4 + 1] << 16) + (bytes[t * 4 + 2] << 8) +
                   bytes[t * 4 + 3];
        for(; t < 80; t++) W[t] = lrot(W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16], 1);

        /* main loop */
        uint32_t temp;
        for(t = 0; t < 80; t++) {
            if(t < 20) {
                K = 0x5a827999;
                f = (b & c) | ((b ^ 0xFFFFFFFF) & d);  // TODO: try using ~
            } else if(t < 40) {
                K = 0x6ed9eba1;
                f = b ^ c ^ d;
            } else if(t < 60) {
                K = 0x8f1bbcdc;
                f = (b & c) | (b & d) | (c & d);
            } else {
                K = 0xca62c1d6;
                f = b ^ c ^ d;
            }
            temp = lrot(a, 5) + f + e + W[t] + K;
            e = d;
            d = c;
            c = lrot(b, 30);
            b = a;
            a = temp;
            // printf( "t=%d %08x %08x %08x %08x %08x\n",t,a,b,c,d,e );
        }
        /* add variables */
        Hn[0] += a;
        Hn[1] += b;
        Hn[2] += c;
        Hn[3] += d;
        Hn[4] += e;
        // printf( "Current: %08x %08x %08x %08x %08x\n",Hn[0],Hn[1],Hn[2],Hn[3],Hn[4] );
        /* all bytes have been processed */
        unprocessedBytes = 0;
    }
    void SHA1::make(const std::string& str, uint8_t* out) {
        SHA1 sha1;
        sha1.addBytes(str.c_str(), str.size());
        sha1.getDigest(out);
    }

    // addBytes **********************************************************
    void SHA1::addBytes(const char* data, int num) {
        assert(data);
        assert(num > 0);
        // add these bytes to the running total
        size += num;
        // repeat until all data is processed
        while(num > 0) {
            // number of bytes required to complete block
            int needed = 64 - unprocessedBytes;
            assert(needed > 0);
            // number of bytes to copy (use smaller of two)
            int toCopy = (num < needed) ? num : needed;
            // Copy the bytes
            memcpy(bytes + unprocessedBytes, data, toCopy);
            // Bytes have been copied
            num -= toCopy;
            data += toCopy;
            unprocessedBytes += toCopy;

            // there is a full block
            if(unprocessedBytes == 64) process();
        }
    }

    // digest ************************************************************
    void SHA1::getDigest(uint8_t* data) {
        // save the message size
        uint32_t totalBitsL = size << 3;
        uint32_t totalBitsH = size >> 29;
        // add 0x80 to the message
        addBytes("\x80", 1);

        uint8_t footer[64] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        // block has no room for 8-byte filesize, so finish it
        if(unprocessedBytes > 56) addBytes((char*)footer, 64 - unprocessedBytes);
        assert(unprocessedBytes <= 56);
        // how many zeros do we need
        const int zero_count = 56 - unprocessedBytes;
        // store file size (in bits) in big-endian format
        storeBigEndianUint32(footer + zero_count, totalBitsH);
        storeBigEndianUint32(footer + zero_count + 4, totalBitsL);
        /*uint32_t* pFooter = (uint32_t*)(footer + zero_count);
         *pFooter++ = htonl(totalBitsH);
         *pFooter = htonl(totalBitsL);*/
        // finish the final block
        addBytes((char*)footer, zero_count + 8);
        // allocate memory for the digest bytes
        // copy the digest bytes
        // uint32_t* p_data = (uint32_t*)data;
        for(int i = 0; i < 5; ++i) {
            storeBigEndianUint32(&data[i * 4], Hn[i]);
            //*p_data = htonl(Hn[i]);
        }
    }
}  // namespace mplc
