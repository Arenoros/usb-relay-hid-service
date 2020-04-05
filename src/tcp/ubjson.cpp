#include <inttypes.h>
#include <string>
#include <vector>
#include <cmath>
#include <cfloat>
struct UBJson {
    size_t capacity;
    size_t size;
    void* data;
};

namespace ubjson {

    enum UBJsonType {
        t_eof,
        t_error,

        t_null = 'Z',
        t_true = 'T',
        t_false = 'F',

        t_int8 = 'i',
        t_uint8 = 'U',
        t_int16 = 'I',
        t_int32 = 'l',
        t_int64 = 'L',
        t_float32 = 'd',
        t_float64 = 'D',
        t_number = 'H',
        t_char = 'C',
        t_string = 'S',

        t_start_obj = '{',
        t_end_obj = '}',
        t_start_arr = '[',
        t_end_arr = ']'
    };

    struct ubj_type {
        template<UBJsonType T>
        size_t size() {
            switch(T) {
            case t_eof:
                return 0;
            case t_error:
                return 0;

            case t_null:
                return 0;
            case t_true:
                return 0;
            case t_false:
                return 0;

            case t_int8:
                return 1;
            case t_uint8:
                return 1;
            case t_int16:
                return 2;
            case t_int32:
                return 4;
            case t_int64:
                return 8;
            case t_float32:
                return 4;
            case t_float64:
                return 8;
            case t_number:
                return 1;
            case t_char:
                return 1;
            case t_string:
                return 1;

            case t_start_obj:
                return 1;
            case t_end_obj:
                return 1;
            case t_start_arr:
                return 1;
            case t_end_arr:
                return 1;
            }
        }
    };

    template<class T>
    struct output {
        std::vector<T> buffer;
        typedef T value_type;
        bool grow(size_t size) {
            return true;
        }

        void write(value_type v) {
            buffer.push_back(v);
        }
    };
    class encoder {
    public:
        typedef output<char> output;

        encoder(output&);

        size_t put();  // Null
        size_t put(bool);
        size_t put(int8_t);
        size_t put(int16_t);
        size_t put(int32_t);
        size_t put(int64_t);
        size_t put(float);
        size_t put(double);
        size_t put(const char*);
        size_t put(const std::string&);

        size_t put_object_begin();
        size_t put_object_end();
        size_t put_array_begin();
        size_t put_array_end();

    private:
        size_t put_token(output::value_type);
        output& buffer;
    };

    encoder::encoder(output& buffer): buffer(buffer){};

    std::size_t encoder::put() { return put_token(t_null); }

    std::size_t encoder::put(bool value) { return put_token(value ? t_true : t_false); }

    std::size_t encoder::put(int8_t value) {
        const output::value_type type(t_int8);
        const std::size_t size = sizeof(type) + sizeof(int8_t);

        if(!buffer.grow(size)) { return 0; }

        buffer.write(type);
        buffer.write(static_cast<output::value_type>(value));

        return size;
    }

    std::size_t encoder::put(int16_t value) {
        const output::value_type type(t_int16);
        const std::size_t size = sizeof(type) + sizeof(int16_t);

        if(!buffer.grow(size)) { return 0; }

        buffer.write(type);
        buffer.write(static_cast<output::value_type>((value >> 8) & 0xFF));
        buffer.write(static_cast<output::value_type>(value & 0xFF));

        return size;
    }

    std::size_t encoder::put(int32_t value) {
        const output::value_type type(t_int32);
        const std::size_t size = sizeof(type) + sizeof(int32_t);

        if(!buffer.grow(size)) { return 0; }

        buffer.write(type);
        buffer.write(static_cast<output::value_type>((value >> 24) & 0xFF));
        buffer.write(static_cast<output::value_type>((value >> 16) & 0xFF));
        buffer.write(static_cast<output::value_type>((value >> 8) & 0xFF));
        buffer.write(static_cast<output::value_type>(value & 0xFF));

        return size;
    }

    std::size_t encoder::put(int64_t value) {
        const output::value_type type(t_int64);
        const std::size_t size = sizeof(type) + sizeof(int64_t);

        if(!buffer.grow(size)) { return 0; }

        buffer.write(type);
        buffer.write(static_cast<output::value_type>((value >> 54) & 0xFF));
        buffer.write(static_cast<output::value_type>((value >> 48) & 0xFF));
        buffer.write(static_cast<output::value_type>((value >> 40) & 0xFF));
        buffer.write(static_cast<output::value_type>((value >> 32) & 0xFF));
        buffer.write(static_cast<output::value_type>((value >> 24) & 0xFF));
        buffer.write(static_cast<output::value_type>((value >> 16) & 0xFF));
        buffer.write(static_cast<output::value_type>((value >> 8) & 0xFF));
        buffer.write(static_cast<output::value_type>(value & 0xFF));

        return size;
    }

    std::size_t encoder::put(float value) {
        const int fpclass = std::fpclassify(value);
        if((fpclass == FP_INFINITE) || (fpclass == FP_NAN)) {
            // Infinity and NaN must be encoded as null
            return put();
        }

        const output::value_type type(t_float32);
        const std::size_t size = sizeof(type) + sizeof(float);

        if(!buffer.grow(size)) { return 0; }

        buffer.write(type);
        // IEEE 754 single precision
        const int32_t ix = 0x00010203;
        int8_t* value_buffer = (int8_t*)&value;
        buffer.write(static_cast<output::value_type>(value_buffer[((int8_t*)&ix)[0]]));
        buffer.write(static_cast<output::value_type>(value_buffer[((int8_t*)&ix)[1]]));
        buffer.write(static_cast<output::value_type>(value_buffer[((int8_t*)&ix)[2]]));
        buffer.write(static_cast<output::value_type>(value_buffer[((int8_t*)&ix)[3]]));

        return size;
    }

    std::size_t encoder::put(double value) {
        const int fpclass = std::fpclassify(value);
        if((fpclass == FP_INFINITE) || (fpclass == FP_NAN)) {
            // Infinity and NaN must be encoded as null
            return put();
        }

        const output::value_type type(t_float64);
        const std::size_t size = sizeof(type) + sizeof(double);

        if(!buffer.grow(size)) { return 0; }

        buffer.write(type);
        // IEEE 754 double precision
        const int64_t ix = 0x0001020304050607;
        int8_t* value_buffer = (int8_t*)&value;
        buffer.write(static_cast<output::value_type>(value_buffer[((int8_t*)&ix)[0]]));
        buffer.write(static_cast<output::value_type>(value_buffer[((int8_t*)&ix)[1]]));
        buffer.write(static_cast<output::value_type>(value_buffer[((int8_t*)&ix)[2]]));
        buffer.write(static_cast<output::value_type>(value_buffer[((int8_t*)&ix)[3]]));
        buffer.write(static_cast<output::value_type>(value_buffer[((int8_t*)&ix)[4]]));
        buffer.write(static_cast<output::value_type>(value_buffer[((int8_t*)&ix)[5]]));
        buffer.write(static_cast<output::value_type>(value_buffer[((int8_t*)&ix)[6]]));
        buffer.write(static_cast<output::value_type>(value_buffer[((int8_t*)&ix)[7]]));

        return size;
    }

    std::size_t encoder::put(const char* value) { return put(std::string(value)); }

    std::size_t encoder::put(const std::string& value) {
        const output::value_type type(t_string);
        const std::string::size_type length = value.size();
        std::size_t size = 0;

        if(length < static_cast<std::string::size_type>(std::numeric_limits<int8_t>::max())) {
            if(buffer.grow(sizeof(type)*2 + sizeof(int8_t) + length)) {
                buffer.write(type);
                size = put(static_cast<int8_t>(length));
                if(size > 0) goto success;
            }
        } else if(length <
                  static_cast<std::string::size_type>(std::numeric_limits<int16_t>::max())) {
            if(buffer.grow(sizeof(type) * 2 + sizeof(int16_t) + length)) {
                buffer.write(type);
                size = put(static_cast<int16_t>(length));
                if(size > 0) goto success;
            }
        } else if(length <
                  static_cast<std::string::size_type>(std::numeric_limits<int32_t>::max())) {
            if(buffer.grow(sizeof(type) * 2 + sizeof(int32_t) + length)) {
                buffer.write(type);
                size = put(static_cast<int32_t>(length));
                if(size > 0) goto success;
            }
        } else {
            if(buffer.grow(sizeof(type) * 2 + sizeof(int64_t) + length)) {
                buffer.write(type);
                size = put(static_cast<int64_t>(length));
                if(size > 0) goto success;
            }
        }
        return 0;

    success:
        for(std::string::const_iterator it = value.begin(); it != value.end(); ++it) {
            buffer.write(*it);
        }

        return sizeof(type) + size + length;
    }

    std::size_t encoder::put_object_begin() { return put_token('{'); }

    std::size_t encoder::put_object_end() { return put_token('}'); }

    std::size_t encoder::put_array_begin() { return put_token('['); }

    std::size_t encoder::put_array_end() { return put_token(']'); }

    std::size_t encoder::put_token(output::value_type value) {
        const std::size_t size = sizeof(value);

        if(!buffer.grow(size)) { return 0; }

        buffer.write(value);

        return size;
    }

}  // namespace ubjson
