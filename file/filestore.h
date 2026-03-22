#ifndef FILE_FILESTORE_H
#define FILE_FILESTORE_H

#include <filesystem>
#include <fstream>
#include "MetadataStore.h"
#include "config/StoreConfig.h"

namespace cdfs{


class FileStore {

#define READ_BUFFER_SIZE (8 * 1024) // 8kb 读缓冲区大小
private:
    StoreConfig config_;
    
    //std::shared_ptr<MetadataStore> metadata_store; // 元数据存储

    bool write_disk(const std::string& real_path, const std::string_view& data);

    bool read_disk(const std::string& real_path, std::string& data);
    std::mutex write_mtx_;                    // 写磁盘互斥锁（避免并发写入冲突)

public:
    explicit FileStore(const StoreConfig& config);
    ~FileStore() = default;
    
    // //两件事情，保存文件，保存元文件
    // std::pair<MetaFile, bool> save_file(const std::string& filename, const std::string& data, 
    //     const std::string& scene = "default", const std::string& file_md5_ = "" );
    
    bool save_file(const std::string_view& data, const std::string& path);

    std::optional<std::string> save_file(const std::string& file_name, const std::string_view& data, const std::string& scene,
         const std::string& md5 = "" );

        
    
    bool save_shard(const std::string file_md5, uint32_t shard_index,
         const std::string& content, const std::string& shard_md5);
        
    //绝不能用于下载接口
    std::optional<std::string> get_file(const std::string& path);

    //bool delete_file(const std::string& file_id);

    std::optional<std::string> get_file_path(const std::string& md5);

   // bool is_file_exist(const std::string& file_id);

    std::optional<MetaFile> get_meta(const std::string& file_id);

    std::vector<std::string>& list_files();
    
    const std::string& get_root_dir() const;

    std::vector<int> get_shard_index(const std::string& file_dir);

    std::string get_temp_dir() const;
};

}


#endif // FILE_FILESTORE_H
