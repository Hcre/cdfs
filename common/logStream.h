#ifndef LOGSTREAM_H_
#define LOGSTREAM_H_



#include <string>
#include <cstring>

namespace cdfs{

const int kSmallBuffer = 4000;
const int kLargeBuffer = 4000 * 1000;

template <int SIZE>
class FixedBuffer {
public:
  FixedBuffer() : current_(buffer_) {}
  void append(const char* data, size_t len);

  const char* data() const { return buffer_; }

  int avail() const {return static_cast<int>(end() - current_);}
  int length() const {return static_cast<int>(current_ - buffer_);}
  void reset() { current_ = buffer_; }
  void brezo() {std::memset(buffer_, 0, sizeof buffer_);}
  char* current() {return current_;}
  void add(int len) {current_ += len;}
private:
  const char* end() const { return buffer_ + sizeof buffer_; }
  char buffer_[SIZE];
  char* current_;
};


class LogStream {
public:
  typedef FixedBuffer<kSmallBuffer> Buffer;
public:
  LogStream() {}

  const Buffer& buffer() { return buffer_; }

  template <typename T>
  void formatInteger(T v);

  LogStream& operator<<(const char* str);
  LogStream& operator<<(const std::string& str);
  LogStream& operator<<(int num);
  // LogStream& operator<<(double const& num);

private:
  //std::string buffer_; //三个问题，性能瓶颈，堆区内存反复申请开销大；线程不安全；类型转化效率低
  Buffer buffer_;

  static const int kMaxNumericSize = 48;
};


class Fmt // : noncopyable
{
 public:
  template<typename T>
  Fmt(const char* fmt, T val);

  const char* data() const { return buf_; }
  int length() const { return length_; }

 private:
  char buf_[32];
  int length_;
};

} // namespace name
#endif
