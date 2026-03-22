#include "MetadataStore.h"
#include "common/logger.h"
#include <nlohmann/json.hpp>
#include <fmt/format.h>
#include <fstream>
#include <stdexcept>
namespace cdfs {




MetadataStore::MetadataStore(const std::string &db_path, const metaConfig &config)
    :config_(config)
{
    std::unique_lock<std::shared_mutex> lock(rw_lock_);
    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::DB* db_ptr = nullptr;
    leveldb::Status status = leveldb::DB::Open(options, db_path, &db_ptr);
    if (!status.ok()) {
        LOG_ERROR << "Failed to open LevelDB: " << status.ToString();
        throw std::runtime_error("Failed to open LevelDB: " + status.ToString());
    }
    db_.reset(db_ptr);
}

MetadataStore::~MetadataStore()
{
    // unique_ptr 会自动调用 leveldb::DB 的析构函数
}

bool MetadataStore::save_meta(const MetaFile &meta){
   
    LOG_INFO << fmt::format("Saving meta: MD5={}, Path={}", meta.md5, meta.path);
    
    // 1. 检查是否存在 (简单去重)
    std::string val;
    if (db_->Get(leveldb::ReadOptions(), meta.md5, &val).ok()) {
        // 如果已存在，可能需要合并 peers，这里简化为直接返回
        return true; 
    }
    std::unique_lock<std::shared_mutex> lock(rw_lock_);
    // 2. 使用 WriteBatch 保证原子性
    leveldb::WriteBatch batch;

    //使用md5作为key，index A
    batch.Put(meta.md5, serialize(meta));

    //Index B, 路径映射
    // 加前缀防止 Key 冲突
    batch.Put("PATH:" + meta.path, meta.md5);

    // Index C: 场景列表 (SCENE:default:1680000:md5 -> MD5)
    // 包含 mtime 是为了让 list_dir 出来的结果按时间排序
    std::string scene_key = "SCENE:" + meta.scene + ":" + std::to_string(meta.mtime) + ":" + meta.md5;
    batch.Put(scene_key, meta.md5);

      // 3. 执行写入
    leveldb::Status s = db_->Write(leveldb::WriteOptions(), &batch);

    if (s.ok()) {
        cached_stat_.file_count++;
        cached_stat_.total_size += meta.size;
        return true;
    }
    return false;
    

}



// bool MetadataStore::save_meta_shard(const MetaFile &meta, uint32_t shard_index, const std::string& file_md5){
//     std::string val;
//     if (db_->Get(leveldb::ReadOptions(), meta.md5, &val).ok()) {
//         // 如果已存在，可能需要合并 peers，这里简化为直接返回
//         return true; 
//     }

//     std::unique_lock<std::shared_mutex> lock(rw_lock_);
//     leveldb::WriteBatch batch;

//     std::string key  = "chunk_meta:" + file_md5 + ":" + std::to_string(shard_index);
//     std::string value = serialize(meta);

//     leveldb::Status s = db_->Put(leveldb::WriteOptions(), key, value);
//     if (s.ok()) {
//         cached_stat_.file_count++;
//         cached_stat_.total_size += meta.size;
//         return true;
//     }
//     return false;

// }


// //查询已上传的的分片ID列表
// std::vector<uint32_t> MetadataStore::get_meta_shard(const std::string &file_id){
//     std::vector<uint32_t> shard_indices;
//     std::string prefix = "chunk_meta:" + file_id + ":";


//     std::shared_lock<std::shared_mutex> lock(rw_lock_);

//      // LevelDB迭代器（有序扫描前缀）
//     std::unique_ptr<leveldb::Iterator> it(db_->NewIterator(leveldb::ReadOptions()));
//     for (it->Seek(prefix); it->Valid() && it->key().ToString().substr(0, prefix.size()) == prefix; it->Next()) {
//         std::string key = it->key().ToString();
//         // 解析出分片索引
//         size_t last_colon = key.rfind(':');
//         if (last_colon != std::string::npos) {
//             std::string index_str = key.substr(last_colon + 1);
//             try {
//                 uint32_t index = std::stoul(index_str);
//                 shard_indices.push_back(index);
//             } catch (const std::exception& e) {
//                 LOG_ERROR << "Failed to parse shard index from key: " << key << ", error: " << e.what();
//             }
//         }
//     }
// }

//获取单个元文件
std::optional<MetaFile> MetadataStore::get_meta(const std::string &key){

    // 读锁
    std::shared_lock<std::shared_mutex> lock(rw_lock_);

    std::string target_md5 = key;
    // 如果 key 不是 MD5 (比如是路径 /group1/...)，先查 Path Index
    if (!is_md5(key)) {
        std::string path_val;
        leveldb::Status s = db_->Get(leveldb::ReadOptions(), "PATH:" + key, &path_val);
        if (!s.ok()) return std::nullopt; // 路径不存在
        target_md5 = path_val; // 拿到真正的 MD5
    }

    std::string val;
    leveldb::Status s = db_->Get(leveldb::ReadOptions(), target_md5, &val);
    if (s.ok()) {
        try {
            LOG_INFO << fmt::format("Retrieved meta for key: {}", key);
            return deserialize(val);
        } catch (...) {
            LOG_ERROR << fmt::format("Failed to deserialize meta for key: {}", key);
            return std::nullopt; // JSON 解析失败
        }
    }

    return std::nullopt; // 未找到
}

bool MetadataStore::save_meta_shard(uint32_t shard_index, const std::string &file_md5, const std::string& shard_md5)
{
   std::string key = "Shard:" + file_md5 + ":" + std::to_string(shard_index);
   std::string value;
   if (!shard_md5.empty()) {
        value = shard_md5;
    }else {
        value = "1";
    }
    std::unique_lock<std::shared_mutex> lock(rw_lock_);
    leveldb::Status s = db_->Put(leveldb::WriteOptions(), key, value);
    if (s.ok()) {
        return true;
    }
    return false;
}

std::vector<uint32_t> MetadataStore::get_uploaded_shard(const std::string &file_id)
{
    if (file_id.empty()) {
        LOG_ERROR << "File ID is empty";  
        return std::vector<uint32_t>();
    }
    std::vector<uint32_t> shard_indices;
    std::string prefix = "Shard:" + file_id + ":";

    std::shared_lock<std::shared_mutex> lock(rw_lock_);

    std::unique_ptr<leveldb::Iterator> it(db_->NewIterator(leveldb::ReadOptions()));
    for (it->Seek(prefix); it->Valid(); it->Next()) {
        std::string key = it->key().ToString();

        if (key.substr(0, prefix.size()) != prefix) {
            break;
        }
        // 2. 从 Key 中提取 Index
        // Key: "TEMP:abc1234:5" -> 提取出 "5"
        std::string index_str = key.substr(prefix.size());
        try {
            uint32_t index = std::stoul(index_str);
            shard_indices.push_back(index);
        }catch (const std::exception& e) {
            LOG_ERROR << "Failed to parse shard index from key: " << key << ", error: " << e.what();
        }
    }
    return shard_indices;
     
}

bool MetadataStore::update_status(const std::string & md5, FileStatus new_status){
    //读写锁
    
    std::unique_lock<std::shared_mutex> lock(rw_lock_);

    std::string json_str;
    leveldb::Status s = db_->Get(leveldb::ReadOptions(), md5, &json_str);

    LOG_INFO << fmt::format("Updating status for MD5: {}, New Status: {}", md5, static_cast<int>(new_status));
    if (!s.ok()) {
        return false; // 未找到
    }

    MetaFile meta = deserialize(json_str);
    meta.status = new_status;
    
    return db_->Put(leveldb::WriteOptions(), md5, serialize(meta)).ok();
}

bool MetadataStore::update_meta(const MetaFile &meta)
{
    delete_meta(meta.md5);
    return save_meta(meta);
}

//删除元文件
bool MetadataStore::delete_meta(const std::string & md5){

    LOG_INFO << fmt::format("Deleting meta for MD5: {}", md5);
    //写锁
    std::unique_lock<std::shared_mutex> lock(rw_lock_);


    //先读出来
    std::string json_str;
    leveldb::Status s = db_->Get(leveldb::ReadOptions(), md5, &json_str);
    if (!s.ok()) {
        return false; // 未找到
    }

    MetaFile meta;
    try {
        meta = deserialize(json_str);
    } catch (...) {
        
        return false;
    }

    // 使用 WriteBatch 保证原子性删除所有相关索引
    leveldb::WriteBatch batch;
    batch.Delete(md5);
    batch.Delete("PATH:" + meta.path);

    std::string scene_key = "SCENE:" + meta.scene + ":" + std::to_string(meta.mtime) + ":" + md5;
    batch.Delete(scene_key);

    s = db_->Write(leveldb::WriteOptions(), &batch);
    if (s.ok()) {
        cached_stat_.file_count--;
        cached_stat_.total_size -= meta.size;
        return true;
    }
    return false;

}

std::vector<MetaFile> MetadataStore::list_dir(const std::string & dir,int offset, int limit, int order){

    LOG_INFO << fmt::format("Listing dir: {}, Offset: {}, Limit: {}, Order: {}", dir, offset, limit, order);
    // 1. 边界条件校验（提前返回，避免无效遍历）
    std::vector<MetaFile> results;
    if (offset < 0 || limit == 0) {
        return results; // offset非法/取0条，直接返回空
    }

    std::shared_lock<std::shared_mutex> lock(rw_lock_);

    // 2. 构造场景前缀（SCENE:<dir>:）
    std::string prefix = "SCENE:" + dir + ":";
    std::unique_ptr<leveldb::Iterator> it(db_->NewIterator(leveldb::ReadOptions()));

    if (order == 1) {
        it->Seek(prefix); // 升序：从前缀第一个Key开始
    } else {
        it->Seek(prefix); // 先定位到前缀起点
        if (!it->Valid()) {
            it->SeekToLast(); // 前缀无数据，跳到最后（后续反向遍历会直接退出）
        } else {
            // 降序：先跳到前缀最后一个Key（大于prefix的第一个Key的前一个）
            std::string upper_bound = prefix;
            // LevelDB Key是字典序，prefix的上界是把最后一个字符+1（简单实现）
            upper_bound.back() += 1;
            it->Seek(upper_bound);
            if (it->Valid()) {
                it->Prev();
            } else {
                it->SeekToLast();
            }
        }
    }

    // 4. 遍历：跳过offset条，再取limit条
    int skipped = 0;    // 已跳过的条数
    int fetched = 0;    // 已获取的条数
    bool stop = false;

    while (it->Valid() && !stop) {
        std::string key = it->key().ToString();

        // 检查前缀匹配
        if (key.compare(0, prefix.size(), prefix) != 0) {
            break; // 前缀不匹配，结束遍历
        }

        if (skipped < offset) {
            skipped++;
        } else {
            // 取出 MD5
            std::string md5 = it->value().ToString();
            auto meta_opt = get_meta(md5);
            if (meta_opt.has_value()) {
                results.push_back(meta_opt.value());
                fetched++;
            }
            if (fetched >= limit && limit > 0) {
                stop = true;
            }
        }

        // 根据顺序移动迭代器
        if (order == 1) {
            it->Next(); // 升序
        } else {
            it->Prev(); // 降序
        }
    }
    return results;
}



SystemStat MetadataStore::get_stat() {
    LOG_INFO << "Getting system statistics";
    std::shared_lock<std::shared_mutex> lock(rw_lock_);
    return cached_stat_;
}

bool MetadataStore::exists(const std::string &file_id)
{
    //复用get_meta方法
    return get_meta(file_id).has_value();
}

void MetadataStore::repair_stat()
{
    LOG_INFO << "Repairing system statistics";
    std::unique_lock<std::shared_mutex> lock(rw_lock_);
    uint64_t count = 0;
    uint64_t size = 0;

    std::unique_ptr<leveldb::Iterator> it(db_->NewIterator(leveldb::ReadOptions()));
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::string key = it->key().ToString();
        // 简单的启发式过滤：主键通常是32位 MD5，不包含冒号
        if (is_md5(key)) {
            try {
                MetaFile m = deserialize(it->value().ToString());
                // 只有正常状态的文件才计入统计
                if (m.status == FileStatus::NORMAL) {
                    count++;
                    size += m.size;
                }
            } catch (...) {}
        }
    }
    cached_stat_.file_count = count;
    cached_stat_.total_size = size;
}

bool MetadataStore::is_enable_trim_filename()
{
    return config_.enable_trim_filename;
}

std::string MetadataStore::get_peer_id()
{
    return config_.peer_id;
}

std::string MetadataStore::get_domain()
{
    return config_.domain; 
}

std::string MetadataStore::serialize(const MetaFile &meta)
{
    try {
        nlohmann::json j = meta;
        return j.dump();
    } catch (const std::exception& e) {
        LOG_ERROR << fmt::format("Serialization error for MetaFile MD5 {}: {}", meta.md5, e.what());
        return "";
    }
}    

MetaFile MetadataStore::deserialize(const std::string &s)
{
    try {
        nlohmann::json j = nlohmann::json::parse(s);
        MetaFile meta = j.get<MetaFile>();
        return meta;
    } catch (const std::exception& e) {
        LOG_ERROR << fmt::format("Deserialization error: {}", e.what());
        throw; // 继续抛出异常，调用方处理
    }
}
}