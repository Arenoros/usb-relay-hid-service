
#include "filetime.h"
#include <time.h>
#include <iomanip>
#include <sstream>

namespace mplc {

#define _1970_1_1 (25569 * 24 * FT_HOUR)
#define DIFF_T_TEMP (531 * 24 * FT_HOUR)  //Не разобрался пока, откуда берется это смещение

#define const_DTdiffFT ((ULONGLONG)94353120000000000ULL)  // 6552300
#define UNIX_DIFF_FT 11644473600000ULL

#define FT_MICROSECOND ((int64_t)10)
#define FT_MILLISECOND (1000 * FT_MICROSECOND)
#define FT_SECOND (1000 * FT_MILLISECOND)
#define FT_MINUTE (60 * FT_SECOND)
#define FT_HOUR (60 * FT_MINUTE)
#define FT_DAY (24 * FT_HOUR)
#define FileTimeName "FileTime"
template<class T>
struct IsZero {
    static const char empty[sizeof(T)];
    static bool check(const T& val) { return memcmp(empty, &val, sizeof(T)) == 0; }
};
template<class T>
const char IsZero<T>::empty[sizeof(T)] = {0};
FileTime::FileTime(int64_t count, Duration duration): file_time(count) {
    if(duration > 0 && duration <= d) file_time *= cv[duration];
}
bool FileTime::isTime() const { return file_time < const_DTdiffFT + DIFF_T_TEMP + _1970_1_1; }

int64_t FileTime::convert(int64_t val, Duration duration) {
    if(duration > 0 && duration <= d) return val / cv[duration];
    return val;
}
bool FileTime::isNull() const { return file_time == 0; }
void FileTime::clear() { file_time = 0; }

int64_t FileTime::tod(Duration d) const {
    int64_t res = file_time % FT_DAY;
    return convert(res, d);
}
const int64_t FileTime::cv[] = {1, FT_MICROSECOND, FT_MILLISECOND, FT_SECOND, FT_MINUTE, FT_HOUR, FT_DAY};

int64_t FileTime::date() const { return file_time - file_time % FT_DAY; }
int64_t FileTime::time(Duration d) const { return convert(file_time, d); }
int64_t FileTime::dt() const { return file_time; }

time_t FileTime::unix(Duration duration) const {
    if(isTime()) return convert(file_time, duration);
    return (file_time - const_DTdiffFT - DIFF_T_TEMP - _1970_1_1) / cv[duration];
}
std::string FileTime::human(const std::string& format) const {
    std::stringstream ss;
    RTIME rt = into<RTIME>();
    bool escape = false;
    for(size_t i = 0; i < format.size(); ++i) {
        if(format[i] == '\'') {
            escape = !escape;
            continue;
        }
        if(format[i] == '\\') {
            ++i;
            if(i < format.size()) ss << format[i];
            continue;
        }
        if(escape) {
            ss << format[i];
            continue;
        }
        switch(format[i]) {
        case 'Y':
            ss << std::setfill('0') << std::setw(4) << rt.year;
            break;
        case 'M':
            ss << std::setfill('0') << std::setw(2) << rt.mon;
            break;
        case 'D':
            ss << std::setfill('0') << std::setw(2) << rt.day;
            break;
        case 'h':
            ss << std::setfill('0') << std::setw(2) << rt.hour % 12;
            break;
        case 'H':
            ss << std::setfill('0') << std::setw(2) << rt.hour;
            break;
        case 'm':
            ss << std::setfill('0') << std::setw(2) << rt.min;
            break;
        case 's':
            ss << std::setfill('0') << std::setw(2) << rt.sec;
            break;
        case 'S':
            ss << std::setfill('0') << std::setw(3) << rt.msec;
            break;
        case 'u':
            ss << std::setfill('0') << std::setw(3) << (file_time % FT_MILLISECOND) / 10;
            break;
        default:
            ss << format[i];
        }
    }
    return ss.str();
}
// ----- From
// Eric S Raymond has a threadsafe version Time, Clock, and Calendar Programming In C

static const int monthdays[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};

template<>
int64_t FileTime::from<struct tm>(const struct tm& t) {
    long year;
    int64_t result;
    static const int MONTHSPERYEAR = 12;
    if(IsZero<tm>::check(t)) return 0;
    /*@ +matchanyintegral @*/
    year = 1900 + t.tm_year + t.tm_mon / MONTHSPERYEAR;
    result = (year - 1970) * 365 + monthdays[t.tm_mon % MONTHSPERYEAR];
    result += (year - 1968) / 4;
    result -= (year - 1900) / 100;
    result += (year - 1600) / 400;
    if((year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0) && (t.tm_mon % MONTHSPERYEAR) < 2) result--;
    result += t.tm_mday - 1;
    result *= 24;
    result += t.tm_hour;
    result *= 60;
    result += t.tm_min;
    result *= 60;
    result += t.tm_sec;
    if(t.tm_isdst == 1) result -= 3600;
    result *= 1000;
    return (result + 11644473600000ULL) * FT_MILLISECOND;
}

template<>
tm FileTime::into<struct tm>() const {
    struct tm res;
    memset(&res, 0, sizeof(struct tm));
    if(file_time < 0) return res;
    uint32_t d, m, y;
    uint32_t date = unix() / (24 * 60 * 60);
    uint32_t t = unix() % (24 * 60 * 60);
    res.tm_wday = (date + 3) % 7;
    date += 720000L;  // days from 0 year to 1971 include leap days

    // some fucking magic
    y = (4L * date - 1L) / 146097L;
    d = (4L * date - 1L - 146097L * y) / 4L;
    date = (4L * d + 3L) / 1461L;
    d = (4L * d + 7L - 1461L * date) / 4L;
    m = (5L * d - 3L) / 153L;
    d = (5L * d + 2L - 153L * m) / 5L;
    y = 100L * y + date;
    if(m < 10L) {
        m += 3L;
    } else {
        m -= 9L;
        y++;
    }

    res.tm_mon = m - 1;
    res.tm_year = y - 1900;
    res.tm_mday = d;
    res.tm_sec = (t % 60L);
    t /= 60L;
    res.tm_hour = (t / 60L);
    res.tm_min = (t % 60L);

    res.tm_yday = monthdays[m - 1] + d;
    if((y % 4) == 0 && ((y % 100) != 0 || (y % 400) == 0) && m > 2) ++res.tm_yday;  // correct leap day
    return res;
}

template<>
int64_t FileTime::from<struct RTIME>(const struct RTIME& t) {
    long year;
    int64_t result;
    static const int MONTHSPERYEAR = 12;
    if(IsZero<RTIME>::check(t)) return 0;
    /*@ +matchanyintegral @*/
    year = t.year;
    result = (year >= 1970 ? (year - 1970) * 365 : 0) + monthdays[(t.mon - 1) % MONTHSPERYEAR];
    result += (year >= 1968 ? (year - 1968) / 4 : 0);
    result -= (year >= 1900 ? (year - 1900) / 100 : 0);
    result += (year >= 1600 ? (year - 1600) / 400 : 0);
    if(year > 1600 && (year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0) && ((t.mon - 1) % MONTHSPERYEAR) < 2)
        result--;

    result += t.day ? t.day - 1 : 0;
    result *= 24;
    result += t.hour;
    result *= 60;
    result += t.min;
    result *= 60;
    result += t.sec;
    result *= 1000;
    result += t.msec;
    if(year > 1600) result += 11644473600000ULL;
    return result * FT_MILLISECOND;
}

template<>
RTIME FileTime::into<struct RTIME>() const {
    struct RTIME res;
    memset(&res, 0, sizeof(struct RTIME));
    if(file_time == 0) return res;
    uint32_t d, m, y;
    uint32_t date = unix() / (24 * 60 * 60);
    uint32_t t = unix() % (24 * 60 * 60);
    date += 720000L;  // days from 0 year to 1971 include leap days

    // some fucking magic
    y = (4L * date - 1L) / 146097L;
    d = (4L * date - 1L - 146097L * y) / 4L;
    date = (4L * d + 3L) / 1461L;
    d = (4L * d + 7L - 1461L * date) / 4L;
    m = (5L * d - 3L) / 153L;
    d = (5L * d + 2L - 153L * m) / 5L;
    y = 100L * y + date;
    if(m < 10L) {
        m += 3L;
    } else {
        m -= 9L;
        y++;
    }
    if(isTime()) {
        y -= 1971;
        m -= 6;
        d -= 16;
    }
    res.year = y;
    res.mon = m;
    res.day = d;
    res.sec = t % 60L;
    t /= 60L;
    res.hour = t / 60L;
    res.min = t % 60L;
    res.msec = unix(ms) % 1000;
    return res;
}

}  // namespace mplc
