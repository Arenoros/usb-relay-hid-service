#pragma once
#include "platform_conf.h"
namespace mplc {
    // date/time
     struct RTIME {
        int16_t year;
        int8_t mon;
        int8_t day;
        int8_t hour;
        int8_t min;
        int8_t sec;
        int8_t msec;
    };
#undef unix
    class FileTime {
        int64_t file_time;
        static const int64_t cv[];
        // Return true if file_time < 1970.1.1
        bool isTime() const;

    public:
        enum Duration { raw, us, ms, s, m, h, d };
        FileTime(): file_time(0) {}
        FileTime(int64_t count, Duration duration = raw);
        explicit FileTime(const FileTime& rv): file_time(rv.file_time) {}
        template<class T>
        FileTime& operator=(const T& ft) {
            file_time = from(ft);
            return *this;
        }
        template<class T>
        FileTime(const T& val): file_time(from(val)) {}
        template<class T>
        operator T() const {
            return into<T>();
        }
        template<class T>
        FileTime operator-(const T& rv) const {
            return file_time - from(rv);
        }
        template<class T>
        FileTime& operator-=(const T& rv) {
            file_time -= from(rv);
            return *this;
        }
        template<class T>
        FileTime operator+(const T& rv) const {
            return file_time + from(rv);
        }
        template<class T>
        bool operator<(const T& rv) const {
            return file_time - from(rv);
        }
        template<class T>
        bool operator>(const T& rv) const {
            return file_time > from(rv);
        }
        template<class T>
        bool operator==(const T& rv) const {
            return file_time == from(rv);
        }
        template<class T>
        bool operator<=(const T& rv) const {
            return file_time <= from(rv);
        }
        template<class T>
        bool operator>=(const T& rv) const {
            return file_time >= from(rv);
        }
        operator bool() const { return file_time != 0; }
        template<class T>
        static int64_t from(const T& val);
        template<class T>
        T into() const;
        bool operator==(const FileTime& rvl) const { return file_time == rvl.file_time; }

        static int64_t convert(int64_t val, Duration duration);
        bool isNull() const;
        void clear();
        //  static int64_t now();
        static FileTime now() { return mplc::getInt64FileTime(); }
        int64_t tod(Duration d = raw) const;
        int64_t date() const;
        int64_t time(Duration d = raw) const;
        int64_t dt() const;
        time_t unix(Duration duration = s) const;
        /* Format string
            '   The escape for text
            Y	The year				2002
            M	The month				April & 04
            D	The day of the month	20
            h	The hour(12-hour time)	12
            H	The hour(24-hour time)	00
            m	The minute				45
            s	The second				52
            S	The millisecond			970
            u	The microsecond			220
        */
        std::string human(const std::string& format = "Y.M.D H:m:s.S\\'u") const;
    };
    template<>
    inline int64_t FileTime::from<FileTime>(const FileTime& val) {
        return val.file_time;
    }
    // ------- Into
    template<>
    inline FileTime FileTime::into<FileTime>() const {
        return *this;
    }
    template<>
    inline int64_t FileTime::from<int64_t>(const int64_t& ft) {
        return ft;
    }
    template<>
    inline int64_t FileTime::from<int>(const int& ft) {
        return ft;
    }
    template<>
    inline int64_t FileTime::from<FILETIME>(const FILETIME& ft) {
        return static_cast<int64_t>(ft.dwHighDateTime) << 32 | static_cast<int64_t>(ft.dwLowDateTime);
    }
    
    template<>
    inline int64_t FileTime::from<double>(const double& ft) {
        return ft * cv[ms];
    }

    // ------- Into
   
    template<>
    inline FILETIME FileTime::into<FILETIME>() const {
        FILETIME ft;
        std::memcpy(&ft, &file_time, sizeof(FILETIME));
        return ft;
    }
    template<>
    inline double FileTime::into<double>() const {
        return static_cast<double>(file_time) / cv[ms];
    }
    template<>
    inline int64_t FileTime::into<int64_t>() const {
        return file_time;
    }
    template<>
    int64_t FileTime::from<tm>(const tm& t);
    template<>
    tm FileTime::into<tm>() const;

    template<>
    int64_t FileTime::from<RTIME>(const RTIME& t);
    template<>
    RTIME FileTime::into<RTIME>() const;

}
