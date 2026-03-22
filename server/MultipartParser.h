#ifndef MULTIPART_PARSER_H
#define MULTIPART_PARSER_H

#include "const.h"

namespace cdfs {
struct HttpContext; 

// 数据结构：承载解析后的结果
struct MultipartForm {
    // 文件相关
    std::string filename;      // 原始文件名 (e.g., "photo.jpg")
    std::string_view content;       // 文件二进制内容
    std::string content_type;  // 文件类型 (e.g., "image/jpeg")

    // 普通参数相关 (scene, path, output, md5 等)
    std::map<std::string, std::string> params;

    // 辅助方法：安全的获取参数
    std::string get_param(const std::string& key, const std::string& default_val = "") const {
        auto it = params.find(key);
        if (it != params.end()) {
            return it->second;
        }
        return default_val;
    }
    
    // 检查是否包含文件
    bool has_file() const {
        return !content.empty();
    }
};

// 解析器类：专门负责处理 multipart/form-data 协议细节
class MultipartParser {
public:
    // 核心静态方法
    // 如果解析失败（格式错误），返回 nullopt
    static std::optional<MultipartForm> parse(const HttpContext& ctx);

private:
    // 内部辅助：从 Header 行中提取 value (例如 name="scene")
    static std::string extract_value(std::string_view header_part, std::string_view key);
};
}

#endif // MULTIPART_PARSER_H