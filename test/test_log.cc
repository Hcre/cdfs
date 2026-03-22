#include "common/logger.h"
#include <iostream>

using namespace cdfs;

int main() {
    Logger::setLogLevel(Logger::LogLevel::TRACE);
    Logger::setOutPutFunc([](const char* msg, int len) {
        std::cout << msg << std::endl; 
    });
    Logger logger(Logger::LogLevel::TRACE, __FILE__, __LINE__);
    logger.stream() << "hello world";
}