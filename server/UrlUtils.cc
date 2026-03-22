#include "UrlUtils.h"

void cdfs::UrlUtils::splitTarget(std::string target, std::string &path, std::string &query){
    // 第一步：先剥离#后的fragment（锚点不参与path/query）
    size_t fragment_pos = target.find('#');
    if (fragment_pos != std::string::npos) {
        target = target.substr(0, fragment_pos);
    }
    
     // 第二步：按?分割path和query
    size_t query_pos = target.find('?');
    if (query_pos == std::string::npos) {
        path = target;
        query.clear();
    }else {
        path = target.substr(0, query_pos);
        query = target.substr(query_pos + 1);
    }

    // 容错：处理空path（如"?a=1" → path为空，query="a=1"）
    if (path.empty() && query_pos == 0) {
        path = "";
    }

}




std::string cdfs::UrlUtils::decode(const std::string &str){
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '+') {
            //+表示空格
            result.push_back(' ');

        } else if (str[i] == '%') {
            // 处理%xx编码：取后两位十六进制转字符
            if (i + 2 >= str.size()) {
                throw UrlParseException("Invalid URL encode: incomplete % sequence (e.g. %2)");
            }
            char hex1 = str[i+1];
            char hex2 = str[i+2];
            if (!std::isxdigit(static_cast<unsigned char>(hex1)) || 
                !std::isxdigit(static_cast<unsigned char>(hex2))) {
                // 可选：抛异常 或 保留原字符（%xx）
                throw UrlParseException("Invalid URL encode: " + std::string(str.substr(i, 3)));
                // 容错方案：result += str[i]; continue;
            }

                // 十六进制转十进制（支持大小写）
            auto hex_to_val = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
                if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
                return 0;
            };
            int val = hex_to_val(hex1) * 16 + hex_to_val(hex2);
            result += static_cast<char>(val);
            i += 2;
        }else {
            result.push_back(str[i]);
        }
    }
    return result;
}

/**
 * @brief 解析query为multimap（保留重复key）
 * @param query 输入串（如"a=1&a=2&b=3"）
 * @return 含重复key的键值对map
 */
std::map<std::string, std::string> cdfs::UrlUtils::parseQuery(const std::string &query){
    std::map<std::string, std::string> params;
    if (query.empty()) {
        return params;
    }

    size_t start = 0;
    while (start < query.size()) {
        size_t end = query.find('&', start);
        std::string pair = query.substr(start, end == std::string::npos ? std::string::npos : end - start);
        start = end == std::string::npos ? std::string::npos : end + 1;

        if (pair.empty()) {
            continue;
        }
        size_t pos = pair.find('=');
        std::string key, value;
        if (pos == std::string::npos) {
            key = decode(pair);
            value = "";
        }else {
            key = pair.substr(0, pos);
            value = decode(pair.substr(pos + 1));
        }
        params[key] = value;
    }
    return params;
}
