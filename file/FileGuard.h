#ifndef FILEGUARD_H
#define FILEGUARD_H
#include "const.h"

namespace {

// 一个简单的清理守卫
struct FileGuard {
    std::string path;
    bool commit = false; // 是否提交（上传成功）

    ~FileGuard() {
        if (!commit && !path.empty()) {
            // 如果没提交（中途异常退出），析构时自动删除临时文件
            std::filesystem::remove(path); 
        }
    }
};

}
#endif