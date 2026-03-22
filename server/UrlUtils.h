#ifndef __URLUTILS_H__
#define __URLUTILS_H__
#include "const.h"

namespace cdfs {

//自定异常
class UrlParseException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};


class UrlUtils {
public:
    //负责将"/path?query" 切开
    static void splitTarget( std::string target, std::string& path, std::string& query);

    //将%xx转为对应的字符
    static std::string decode(const std::string& str);

    //只负责把 "a=1&b=2" 变成 map
    static std::map<std::string, std::string> parseQuery(const std::string& query);



};

}

#endif