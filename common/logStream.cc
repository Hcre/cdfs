#include "logStream.h"
#include "Type.h"
#include <cassert>

namespace cdfs{

template <int SIZE>
void FixedBuffer<SIZE>::append(const char* str, size_t len) {
    if (implicit_cast<size_t>(avail()) < len) {
        return;
    }
    std::memcpy(current_, str, len);
    current_ += len;
}

LogStream &LogStream::operator<<(const char *str)
{
    if (str) {
        buffer_.append(str, strlen(str));
    }else {
        buffer_.append("(null)", 6);
    }
    return *this;
}

LogStream &LogStream::operator<<(const std::string &str)
{
    buffer_.append(str.data(), str.size());
    return *this;
}

LogStream &LogStream::operator<<(int num)
{
    formatInteger(num);
    return *this;
}


template <typename T>
void LogStream::formatInteger(T v){
    if (buffer_.avail() > kMaxNumericSize) {
        char* buf = buffer_.current();
        int len = sprintf(buf, "%d", v);
        buffer_.add(len);
    }
}


template<typename T>
Fmt::Fmt(const char* fmt, T val)
{
  static_assert(std::is_arithmetic<T>::value == true, "Must be arithmetic type");

  length_ = snprintf(buf_, sizeof buf_, fmt, val);
  assert(static_cast<size_t>(length_) < sizeof buf_);
}

// Explicit instantiations

template Fmt::Fmt(const char* fmt, char);

template Fmt::Fmt(const char* fmt, short);
template Fmt::Fmt(const char* fmt, unsigned short);
template Fmt::Fmt(const char* fmt, int);
template Fmt::Fmt(const char* fmt, unsigned int);
template Fmt::Fmt(const char* fmt, long);
template Fmt::Fmt(const char* fmt, unsigned long);
template Fmt::Fmt(const char* fmt, long long);
template Fmt::Fmt(const char* fmt, unsigned long long);

template Fmt::Fmt(const char* fmt, float);
template Fmt::Fmt(const char* fmt, double);



} // namespace cdfs
