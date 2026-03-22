#ifndef __META_CONFIG_H__
#define __META_CONFIG_H__
#include "const.h"

struct metaConfig
{
    std::string group = "group1";                 // 分组（对应 Go 的 Config().Group）
    std::string peer_id = "node_01";              // 集群节点ID（对应 Go 的 PeerId）
    std::string domain = "http://192.168.88.131:8080";        // 下载域名+端口
    //std::vector<std::string> allow_scenes = {"default", "avatar", "document"}; // 允许的场景（对应 Go 的 Scenes）
    std::vector<std::string> allow_extensions = {".jpg", ".png", ".txt", ".pdf"}; // 允许的文件扩展名
    bool enable_trim_filename = true;             // 是否过滤文件名特殊字符（对应 Go 的 EnableTrimFileNameSpecialChar）
    bool enable_distinct_file = true;       
};



#endif