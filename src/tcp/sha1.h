/* sha1.h

Copyright (c) 2005 Michael D. Leonhard

http://tamale.net/

This file is licensed under the terms described in the
accompanying LICENSE file.
*/
#ifndef SHA1_HEADER
#define SHA1_HEADER
#include <cstdint>
#include <array>

namespace mplc {

    class SHA1 {
        uint32_t Hn[5];
        uint8_t bytes[64];
        int unprocessedBytes;
        uint32_t size;
        void process();

    public:
        SHA1();
        ~SHA1();
        static void make(const std::string& str, uint8_t* out);
        void addBytes(const char* data, int num);
        void getDigest(uint8_t* data);
        // utility methods

        static void storeBigEndianUint32(uint8_t* byte, uint32_t num);
        static uint32_t lrot(uint32_t x, int bits);
        static void hexPrinter(uint8_t* c, int l);
    };
    void to_base64(const uint8_t* data, size_t data_size, size_t b64_size, char* out);
    void from_base64(const char* b64, size_t b64_size, size_t data_size, uint8_t* out);
    template<class T>
    std::string to_base64(const T& _data) {
        static const size_t b64_size = 4 * ((sizeof(T) + 2) / 3);
        static const size_t data_size = sizeof(T);
        char out[4 * ((sizeof(T) + 2) / 3)] = {0};
        const uint8_t* ref = reinterpret_cast<const uint8_t*>(&_data);
        // uint8_t buf[sizeof(T)] = { 0 };
        to_base64(ref, data_size, b64_size, out);
        return std::string(out, b64_size);
    }

    template<class T>
    T from_base64(const std::string& b64) {
        static const size_t b64_size = 4 * ((sizeof(T) + 2) / 3);
        static const size_t data_size = sizeof(T);
        uint8_t decoded_data[sizeof(T)] = {0};
        if(b64.size() != b64_size)
            perror("Error parse base64 \n");
        else {
            from_base64(b64.c_str(), b64_size, data_size, decoded_data);
        }
        return *reinterpret_cast<T*>(&decoded_data);
    }

}  // namespace mplc

#endif
