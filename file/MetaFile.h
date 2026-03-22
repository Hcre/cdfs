#ifndef __METAFILE_H__
#define __METAFILE_H__
#include "const.h"
#include <nlohmann/json.hpp>

namespace cdfs{

enum class FileStatus {
    NORMAL = 0,       // 正常
    DELETED = 1,      // 已删除
    CORRUPTED = 2,    // 损坏
    SYNCING = 3,      // 同步中 (分片上传未完成)
    LOCKED = 4        // 锁定
};

    // 1. 定义单个分片的信息
struct FileShard {
    uint32_t index;         // 分片序号 (0, 1, 2...)
    std::string shard_id;   // 分片物理文件名 (如 fileID_0)
    uint64_t size;          // 分片大小
    std::string md5;        // 分片内容的 MD5 (用于单独校验分片完整性)
    std::string path;      // 分片的存径（如 /data/shards/xxx_md5_0）
    
    // 分片序列化宏
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(FileShard, index, shard_id, size, md5)
};



struct MetaFile {
    // --- 核心索引 ---
    std::string md5;        // 整个文件的 MD5
    std::string path;       // 逻辑路径,迁移拼接


    // --- 基础信息 ---
    std::string name;
    std::string domain;
    std::string scene;
    std::string src; //来源？
    int64_t size = 0;
    int64_t mtime = 0;

     // --- 【补全状态字段】 ---
    FileStatus status = FileStatus::NORMAL; 

    // --- 分片支持字段 (新增) ---
    bool is_sharded = false;        // 是否启用了分片
    uint64_t shard_std_size = 0;    // 标准分片大小 (例如 10MB)
    uint32_t shard_count = 0;       // 分片总数
    std::vector<FileShard> shards;  // 分片列表

    int32_t uploaded_count = 0;         // 已上传分片数
    std::vector<uint32_t> uploaded_chunks; // 已上传分片ID列表（断点续传核心）

    // --- 集群 ---
    std::vector<std::string> peers; 

    // --- 序列化宏 (注意要把 shards 加进去) ---
    // nlohmann/json 会自动处理 vector<FileShard> 的递归序列化
    NLOHMANN_DEFINE_TYPE_INTRUSIVE(MetaFile, md5, path,    
                                    name, domain, scene, src, size, mtime,status, 
                                   is_sharded, shard_std_size, shard_count, shards,
                                   uploaded_count, uploaded_chunks, peers)
};


// 对应 API: /group/stat
struct SystemStat {
    uint64_t file_count = 0;  // 文件总数
    uint64_t total_size = 0;  // 占用总空间
};

}



#endif