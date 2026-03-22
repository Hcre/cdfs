#ifndef CDFS_MIMEMAP_H
#define CDFS_MIMEMAP_H

#include <unordered_map>
#include <string>
#include <filesystem>
#include <cctype>    // 添加这个头文件
#include <algorithm> // 添加这个头文件

namespace cdfs {

class MimeMap {
public:
    static std::string get_mime_type(const std::filesystem::path& file_path) {
        // 获取文件后缀（包含点号，如 ".jpg"），并转换为小写
        std::string extension = file_path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        
        // 在映射表中查找
        auto it = get_mime_map().find(extension);
        if (it != get_mime_map().end()) {
            return it->second;
        }
        
        // 未找到映射，返回默认的二进制流类型
        return "application/octet-stream";
    }

private:
    // 使用静态函数返回映射表，确保只初始化一次
    static const std::unordered_map<std::string, std::string>& get_mime_map() {
        static const std::unordered_map<std::string, std::string> MIME_TYPE_MAP = {
            // 图片
            {".jpg", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".png", "image/png"},
            {".gif", "image/gif"},
            {".bmp", "image/bmp"},
            {".webp", "image/webp"},
            {".svg", "image/svg+xml"},
            {".ico", "image/x-icon"},
            // 文本/代码
            {".html", "text/html"},
            {".htm", "text/html"},
            {".css", "text/css"},
            {".js", "application/javascript"},
            {".json", "application/json"},
            {".txt", "text/plain"},
            {".md", "text/markdown"},
            // 字体
            {".woff", "font/woff"},
            {".woff2", "font/woff2"},
            {".ttf", "font/ttf"},
            // 应用
            {".pdf", "application/pdf"},
            {".zip", "application/zip"},
            {".gz", "application/gzip"},
            // 视频/音频
            {".mp4", "video/mp4"},
            {".mp3", "audio/mpeg"},
            {".wav", "audio/wav"}
            // 注意：这里没有默认的 "" 条目，因为我们在函数中处理了未找到的情况
        };
        return MIME_TYPE_MAP;
    }
};

} // namespace cdfs

#endif // CDFS_MIMEMAP_H