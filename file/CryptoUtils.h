#ifndef __CRYPTO_UTILS_H__
#define __CRYPTO_UTILS_H__

#include "const.h"
#include <iomanip>
#include <openssl/md5.h> 
#include <regex>




namespace cdfs {

class CryptoUtils {
public:
    inline static std::string md5(const std::string& data) {
        unsigned char result[MD5_DIGEST_LENGTH];
        MD5((unsigned char*)data.data(), data.size(), result);

        std::stringstream ss;
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)result[i];
        }
        return ss.str();
    }

    inline static std::string md5(const std::string_view& data) {
        unsigned char result[MD5_DIGEST_LENGTH];
        MD5((unsigned char*)data.data(), data.size(), result);

        std::stringstream ss;
        for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)result[i];
        }
        return ss.str();
    }


    // 过滤文件名特殊字符（对应 Go 的 TrimFileNameSpecialChar）
    inline static std::string trim_filename_special_char(const std::string& filename) {
        // 1. 过滤非法特殊字符（原始字符串格式：R"(内容)"）
        std::regex reg_special(R"([\/\\\:\*\?\"\<\>\|\(\)\[\]\{\}\,\;\'\#\@\%\&\$\^\~\=\+\-\!\~\s])");
        std::string res = std::regex_replace(filename, reg_special, "");
        
        // 2. 移除连续的点（核心修正：原始字符串必须用 R"(内容)" 包裹）
        std::regex reg_dot(R"(\.{2,})");  // 匹配 2 个及以上连续的点
        res = std::regex_replace(res, reg_dot, "");
        
        // 可选优化：移除首尾的单个点（避免文件名以 . 结尾/开头，比如 ".test.." → "test"）
        if (!res.empty() && res.front() == '.') {
            res.erase(res.begin());
        }
        if (!res.empty() && res.back() == '.') {
            res.pop_back();
        }
        
        return res;
    }

    // 校验文件扩展名（对应 Go 端的 Extensions 白名单）
    inline static bool check_file_ext(const std::string& filename, const std::vector<std::string>& allow_ext) {
        if (allow_ext.empty()) return true; // 无白名单则放行
        std::string ext = std::filesystem::path(filename).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower); // 转小写
        return std::find(allow_ext.begin(), allow_ext.end(), ext) != allow_ext.end();
    }

    // 校验场景合法性（对应 Go 端的 CheckScene）
    inline static bool check_scene(const std::string& scene, const std::vector<std::string>& allow_scenes) {
        if (allow_scenes.empty()) return true;
        return std::find(allow_scenes.begin(), allow_scenes.end(), scene) != allow_scenes.end();
    }

    // 辅助：MD5 映射到 2级物理目录（优化跨平台路径）
    // MD5: a1b2c3d4... → a1/b2/a1b2c3d4...
    inline static std::string md5_to_rel_path(const std::string& md5) {
        if (md5.length() < 4) {
            throw std::invalid_argument("[FileStore] MD5 length too short: " + md5);
        }
        std::filesystem::path p;
        p /= md5.substr(0, 2);   // 一级目录：前2位
        p /= md5.substr(2, 2);   // 二级目录：3-4位
        p /= md5;                // 文件名：完整 MD5
        return p.string();       // 自动适配 Windows/Linux 路径分隔符
    }


};

}

#endif 