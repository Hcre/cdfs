#ifndef SHARDED_TRANSFER_CONFIG_H
#define SHARDED_TRANSFER_CONFIG_H

#include "const.h"

// ========== 分片传输配置 ==========
struct ShardedTransferConfig {
    uint64_t default_shard_size = 10 * 1024 * 1024; // 默认标准分片大小：10MB
    uint64_t min_shard_size = 1 * 1024 * 1024;       // 最小分片大小：1MB（防止过小分片）
    
    int64_t task_expire_seconds = 3600;             // 分片任务超时时间：1小时
    std::string shard_scene_prefix = "shard_";       // 分片场景前缀（隔离完整文件）
    bool auto_clean_expired = true;                 // 是否自动清理过期任务
    std::string merge_file_dir = "./data/store";  // 合并后文件存储目录
    std::string temp_shard_dir = "./data/temp"; // 分片临时存储目录（合并前的分片文件）
};



#endif