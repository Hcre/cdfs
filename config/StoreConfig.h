#ifndef CDFS_CONFIG_STORECONFIG_H
#define CDFS_CONFIG_STORECONFIG_H

#include "const.h"

namespace cdfs {

struct StoreConfig {
    std::string root_dir = "./data/store";       // 存储根目录（对应 Go 的 STORE_DIR）
    std::string temp_dir = "_tmp";        // 临时文件目录（对应 Go 的 TEMP_DIR）

    uint64_t max_file_size = 10 * 1024 * 1024;
    std::string group = "group1";                 // 分组（对应 Go 的 Config().Group）
    std::string peer_id = "node_01";              // 集群节点ID（对应 Go 的 PeerId）
    std::string domain = "localhost:8080";        // 下载域名+端口
    //std::vector<std::string> allow_scenes = {"default", "avatar", "document"}; // 允许的场景（对应 Go 的 Scenes）
    std::vector<std::string> allow_extensions = {".jpg", ".png", ".txt", ".pdf", ".zip", ".file"}; // 允许的文件扩展名
    bool enable_trim_filename = true;             // 是否过滤文件名特殊字符（对应 Go 的 EnableTrimFileNameSpecialChar）
    bool enable_distinct_file = true;             // 是否开启文件去重（对应 Go 的 EnableDistinctFile）
};

}

#endif