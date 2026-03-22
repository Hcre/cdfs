#include "logger.h"
#include <cassert>

namespace cdfs{

Logger::LogLevel g_logLevel = Logger::LogLevel::INFO; 
Logger::OutPutFunc Logger::g_output = nullptr;



// 1. 线程局部变量：定义（thread_local 保留）
thread_local time_t _lastSecond = 0;
thread_local char _time[32] = {0};

void SetLogLevel(Logger::LogLevel level) {
    g_logLevel = level;
}


const char* LogLevelName[Logger::NUM_LOG_LEVELS] =
{
  "TRACE ",
  "DEBUG ",
  "INFO  ",
  "WARN  ",
  "ERROR ",
  "FATAL ",
};

Logger::Logger(LogLevel level, const char *file, int line):
    impl_(level, file, line) {
        
}

Logger::Logger(const char *file, int line):
    impl_(LogLevel::INFO, file, line) {
}

Logger::~Logger(){
    if (impl_.level_ < g_logLevel) return;
    finish();
    //输出到asyncLogging线程
    const LogStream::Buffer& buffer(stream().buffer());
    g_output(buffer.data(), buffer.length());
    
}


void Logger::finish()
{
    stream() << " -" << impl_.filename_ << ":" << impl_.line_ << "\n";
}

//格式化时间
void Logger::Impl::formatTime()
{
    Timestamp now = Timestamp::now();
    int64_t Microseconds = now.microsecondsSinceEpoch();
    time_t seconds = static_cast<time_t>(Microseconds / Timestamp::kMicroSecondsPerSecond);
    int microseconds = static_cast<int>(Microseconds % Timestamp::kMicroSecondsPerSecond);

    if (seconds != _lastSecond) {
        _lastSecond = seconds;
        struct tm tm;
        localtime_r(&seconds, &tm);
        int len = snprintf(_time, sizeof(_time), "%4d%02d%02d %02d:%02d:%02d",
        tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
        assert(len == 17);
        // if (len == 17) {
        //     fprintf(stderr, "Time format len=%d, content='%s'\n", len, _time);
        // }

        Fmt micorFmt(".%06d", microseconds);
        stream_ << _time << micorFmt.data();
    }

}

Logger::Impl::Impl(LogLevel level, const char *file, int line)
    : level_(level), filename_(file), line_(line) {
    formatTime();
    //2、线程id
    //3、level
    stream_ << " " << LogLevelName[level];
}

} // namespace cdfs
