#ifndef CDFS_LOGER_H
#define CDFS_LOGER_H


#include <functional>
#include "logStream.h"
#include "Timestamp.h"

namespace cdfs {

// 先定义日志级别映射（和你的 Logger::LogLevel 对齐）
#define LOG_LEVEL_TRACE Logger::LogLevel::TRACE
#define LOG_LEVEL_DEBUG Logger::LogLevel::DEBUG
#define LOG_LEVEL_INFO  Logger::LogLevel::INFO
#define LOG_LEVEL_WARN  Logger::LogLevel::WARN
#define LOG_LEVEL_ERROR Logger::LogLevel::ERROR

// 核心日志宏：封装流式输出，自动传文件/行号，符合 Muduo 风格
#define LOG_BASE(level) \
  Logger(level, __FILE__, __LINE__).stream()

// 各级别日志宏（直接流式调用，和 Muduo 完全一致）
#define LOG_TRACE LOG_BASE(LOG_LEVEL_TRACE)
#define LOG_DEBUG LOG_BASE(LOG_LEVEL_DEBUG)
#define LOG_INFO  LOG_BASE(LOG_LEVEL_INFO)
#define LOG_WARN  LOG_BASE(LOG_LEVEL_WARN)
#define LOG_ERROR LOG_BASE(LOG_LEVEL_ERROR)

// 日志初始化宏（仅需调用一次）
#define LOG_INIT() do { \
  Logger::setLogLevel(LOG_LEVEL_INFO); \
  Logger::setOutPutFunc([](const char* msg, int len) { \
    std::cout.write(msg, len); \
    std::cout << std::endl; \
  }); \
} while(0)




// 1. 线程局部变量：仅声明（加 extern），定义移到 .cc
extern thread_local time_t _lastSecond;
extern thread_local char _time[32];

class Logger {   
public:
  enum LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARN = 3,
    ERROR = 4,
    FATAL = 5,
    NUM_LOG_LEVELS = 6
  };



////获取简短的文件名
// class sourFile {
//  public:
//   sourFile(const char* file) : filename(file) {}
//  private:
//   std::string filename;
// };


public:
  Logger(LogLevel level = INFO, const char* file = "", int line = 0);
  Logger(const char* file, int line);
  ~Logger();

  void finish();
  LogStream& stream() {return impl_.stream_;}

  static LogLevel logLevel();
  static void setLogLevel(LogLevel level);

  // 设置回调传递到asyncLogging
  typedef void(*OutPutFunc)(const char* msg, int len);
  static void setOutPutFunc(OutPutFunc);
  static OutPutFunc g_output;
class Impl {
public:
  void formatTime();

  Impl(LogLevel level, const char* file, int line);


  Logger::LogLevel level_; //日志等级
  LogStream stream_; //
  Timestamp timestamp_; //时间
  std::string filename_; //文件名
  int line_; //行号
  int tid_; //线程号

};

  Impl impl_;
    
};

extern Logger::LogLevel g_logLevel;

// 大于此等级的日志可输出
inline Logger::LogLevel Logger::logLevel() {
    return g_logLevel;
}

inline void Logger::setLogLevel(LogLevel level)
{
  g_logLevel = level;
}

inline void Logger::setOutPutFunc(OutPutFunc func)
{
  g_output = func;
}
}

#endif

