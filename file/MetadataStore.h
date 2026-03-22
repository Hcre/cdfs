#ifndef __METADATASTORE_H__
#define __METADATASTORE_H__


#include <leveldb/write_batch.h>
#include <leveldb/db.h>
#include <shared_mutex> // C++17 读写锁
#include "config/metaConfig.h"
#include "MetaFile.h"

namespace cdfs {

class MetadataStore {
private:
    std::unique_ptr<leveldb::DB> db_;
    metaConfig config_;

  // 读写锁：保护统计数据和 DB 并发访问
    mutable std::shared_mutex rw_lock_;
    
    // 内存中的统计缓存 (避免每次都全库扫描)
    SystemStat cached_stat_;

    // 辅助函数
    bool is_md5(const std::string& s) { return s.length() == 32; }
    std::string serialize(const MetaFile& meta); 
    MetaFile deserialize(const std::string& s);

    std::string buffer_;
public:
    //构造时打开数据库
    explicit MetadataStore(const std::string& db_path, const metaConfig& config = metaConfig()); 
    //析构关闭
    ~MetadataStore();

    // 禁止拷贝 (LevelDB 指针不能随便拷)
    MetadataStore(const MetadataStore&) = delete;
    MetadataStore& operator=(const MetadataStore&) = delete;

    //保存元数据
    bool save_meta(const MetaFile& meta);

    //保存分片元数据
    // bool save_meta_shard(const MetaFile& meta, uint32_t shard_index, const std::string& file_md5);

    // std::vector<uint32_t> get_meta_shard(const std::string& file_id);

    // // 2. 获取元数据
    // // 内部逻辑：LevelDB Get -> JSON/Protobuf -> MetaFile
    std::optional<MetaFile> get_meta(const std::string& file_id);

    bool save_meta_shard(uint32_t shard_index, const std::string& file_md5, const std::string& shard_md5 = "");

    //返回已经上传的分片数
    std::vector<uint32_t> get_uploaded_shard(const std::string& file_id);

    // 2.5 更新文件状态
    bool update_status(const std::string& file_id, FileStatus new_status);

    bool update_meta(const MetaFile& meta);

    // 3. 删除元数据
    bool delete_meta(const std::string& file_id);

    SystemStat get_stat(); 

    std::vector<MetaFile> list_dir(const std::string& dir, int offset, int limit, int order = 1); //1为升，2为降

    // 4. 检查是否存在
    bool exists(const std::string& file_id);

    void repair_stat();

    //todo 看一个文件夹的文件信息

    bool is_enable_trim_filename();

    std::string get_peer_id();

    std::string get_domain();

};


}

#endif