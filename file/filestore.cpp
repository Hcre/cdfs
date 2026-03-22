#include "filestore.h"
#include "common/logger.h"
#include "file/CryptoUtils.h"
#include <fmt/format.h>
namespace cdfs {

// 临时文件名生成器
std::string generate_temp_suffix() {
    static thread_local std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int> distribution(100000, 999999);
    return std::to_string(distribution(generator));
}

FileStore::FileStore(const StoreConfig& config)
    : config_(config) {

        // 确保根目录存在（递归创建）
    try {
        std::filesystem::create_directories(config_.root_dir);
         // 确保临时目录/大文件目录存在（对齐 Go 端的 _tmp/_big）
        std::filesystem::create_directories(config_.root_dir + "/_tmp");
        std::filesystem::create_directories(config_.root_dir + "/_big");
        LOG_INFO << "FileStore initialized with root directory: " << config_.root_dir;
    } catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR << "Failed to create root directory: " << e.what();
        throw;
    }
}
bool FileStore::write_disk(const std::string& full_path, const std::string_view& data) {

     // 1. 【性能优化】先做一次快速检查
    // 如果文件已经存在（说明刚才有别的线程已经写好了），直接返回成功
    // 虽然这里有竞态（可能刚查完就被删了），但即使往后走也是安全的
    if (std::filesystem::exists(full_path)) {
        if (std::filesystem::file_size(full_path) == data.size()) {
            return true; 
        }
    }

    // 2. 准备目录 (同之前)
    std::filesystem::path final_path(full_path);
    std::filesystem::path parent_dir = final_path.parent_path();
    std::error_code ec;
    std::filesystem::create_directories(parent_dir, ec); // 忽略已存在的错误

    // 3. 【关键】生成唯一的临时文件路径
    // 格式: /path/to/file.jpg.tmp.123456
    std::filesystem::path temp_path = final_path;
    temp_path += ".tmp." + generate_temp_suffix();

    try {
        // 4. 写临时文件 (这是线程私有的，没人跟你抢！)
        {
            std::ofstream ofs(temp_path, std::ios::binary | std::ios::out);
            if (!ofs) return false;
            //改进方向（未来优化）：上传接口也需要改成“流式”的。即 parse_multipart 不要把数据存 string，
            //而是解析出一块 buffer 就调用一次 ofstream::write。
            //这就是我在前面提到的“高级接口：分片上传”的意义所在 —— 分片上传通过把大文件切成小块（5MB），完美规避了这个问题。
            ofs.write(data.data(), data.size());
            if (!ofs.good()) {
                ofs.close();
                std::filesystem::remove(temp_path);
                return false;
            }
            ofs.close(); // 必须先关闭才能 rename
        }

        // 5. 【关键】原子重命名 (Atomic Rename)
        // 将临时文件移动为正式文件。
        // 如果 final_path 已经存在（被别的线程抢先生成了），rename 会原子性地覆盖它
        // 因为内容一样，覆盖也无所谓。
        std::filesystem::rename(temp_path, final_path);

    // ========== 原有文件完整性校验 ==========
    std::filesystem::file_status st = std::filesystem::status(full_path);
    if (!std::filesystem::exists(st) || std::filesystem::file_size(full_path) != data.size()) {
        std::filesystem::remove(full_path); // 删除不完整文件
        LOG_ERROR << "File size mismatch after write: " << full_path
                << " (expected: " << data.size() << ", actual: " << std::filesystem::file_size(full_path) << ")";
        return false;
    }
    } catch (const std::filesystem::filesystem_error& e) {
        // 单独捕获文件系统异常（目录创建/文件操作失败），更精准
        LOG_ERROR << "Filesystem exception during write: " << e.what()
                  << " (path: " << full_path << ")";
        std::filesystem::remove(full_path); // 清理不完整文件
        return false;
    } catch (const std::exception& e) {
        // 捕获其他通用异常
        LOG_ERROR << "General exception during file write: " << e.what();
        std::filesystem::remove(full_path); // 清理不完整文件
        return false;
    }
    return true;
}

bool FileStore::read_disk(const std::string &real_path, std::string &data)
{
    namespace fs = std::filesystem;
    std::error_code ec;

     // 1. 检查文件是否存在且是普通文件 (非目录)
    // 使用 ec 版本避免抛出异常，性能更好
    if (!fs::exists(real_path, ec) || !fs::is_regular_file(real_path, ec)) {
        LOG_ERROR << "File read error: not found or not a file: " << real_path;
        return false;
    }

     // 2. 获取文件大小
    uintmax_t file_size = fs::file_size(real_path, ec);
    if (ec) {
        LOG_ERROR << "Get file size failed: " << real_path;
        return false;
    }

    if (file_size == 0) {
        data.clear();
        return true; // 空文件也是文件
    }

        // 3. 打开文件流 (二进制模式)
    std::ifstream file(real_path, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR << "Open file failed: " << real_path;
        return false;
    }

     // 4. [性能关键] 预分配内存 + 直接读取
    try {
        // resize 会分配内存，如果 file_size 巨大(如 10GB) 可能会抛 bad_alloc
        data.resize(file_size);
        
        // 直接读入 string 的内部 buffer
        // &data[0] 或 data.data() 获取可写指针 (C++17 之后 data() 返回 char*)
        file.read(data.data(), file_size);

        // 检查实际读取字节数是否匹配
        if (!file) {
            LOG_ERROR << "Read file partial/fail: " << real_path;
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR << "Read file exception (OOM?): " << e.what();
        return false;
    }

    return true;

}

// // 核心上传逻辑,前端只需传入名字和数据，扩展一个场景
// std::pair<MetaFile, bool> FileStore::save_file(const std::string& filename,
//      const std::string& data, std::string const& scene, const std::string& file_md5_) {
    
//     //参数校验
//     if (!CryptoUtils::check_scene(scene, config_.allow_scenes)) {
//         LOG_ERROR << "Scene not allowed: " << scene;
//         throw std::invalid_argument("[FileStore] Invalid scene: " + scene);
//     }
    
    // std::string clean_name = filename;
    // if (config_.enable_trim_filename) {
    //     clean_name = CryptoUtils::trim_filename_special_char(filename);
    // }

    // if (CryptoUtils::check_file_ext(filename, config_.allow_extensions) == false) {
    //     LOG_ERROR << "File extension not allowed: " << filename;
    //     throw std::invalid_argument("[FileStore] Invalid scene: " + scene);
    // }

//     if (clean_name.empty()) {
//         LOG_ERROR << "Filename is empty after trimming special characters.";
//         throw std::invalid_argument("[FileStore] Filename is empty after trimming special characters.");
//     }

//     //计算md5
//     std::string file_md5 = CryptoUtils::md5(data);
//     if (file_md5.empty()) {
//         throw std::runtime_error("[FileStore] MD5 calculation failed");
//     }

//     if (file_md5_ != "" && file_md5_ != file_md5) {
//         LOG_ERROR << "Provided MD5 does not match calculated MD5.";
//         throw std::invalid_argument("[FileStore] Provided MD5 does not match calculated MD5.");
//     }

//     //去重检查,秒传
//     auto existing_meta = metadata_store->get_meta(file_md5);
//     if (config_.enable_distinct_file && existing_meta.has_value()) {
//         LOG_INFO << "File already exists, deduplication enabled. MD5: " << file_md5;
        
//         //构造新的元数据，更新逻辑路径/文件名/修改时间
//         MetaFile new_meta = existing_meta.value();
//         new_meta.name = clean_name;
//         new_meta.scene = scene;
//         new_meta.mtime = std::time(nullptr);
//         // 标准化path（分层+过滤后的文件名，无URL）
//         std::string md5_prefix1 = file_md5.substr(0, 2);
//         std::string md5_prefix2 = file_md5.substr(2, 2);
//         new_meta.path = fmt::format("/{}/{}/{}/{}/{}",
//                                    scene, md5_prefix1, md5_prefix2, file_md5, clean_name);
        

//         auto last = std::unique(new_meta.peers.begin(), new_meta.peers.end());
//         new_meta.peers.erase(last, new_meta.peers.end());


//         // 保存更新后的元数据
//         if (!metadata_store->save_meta(new_meta)) {
//             throw std::runtime_error("[FileStore] Save meta failed (deduplication)");
//         }

//         return {new_meta, false}; // false表示未新写入数据
//     }


//      // ==========================================
//     // 3. 核心步骤3：生成「可拼接域名的规范path」（关键修改）
//     // ==========================================
//     // 规则：/场景/MD5前缀/原始文件名（保证唯一性 + 可直接拼接域名）
//     // 示例：scene=default → /default/a1/b2/a1b2xxxx/test.jpg
//     std::string md5_prefix1 = file_md5.substr(0, 2);  // 前2位（目录分层，避免单目录文件过多）
//     std::string md5_prefix2 = file_md5.substr(2, 2);  // 2-4位
//     // 最终path：/scene/前缀1/前缀2/MD5/原始文件名（规范相对路径，无绝对路径）
//     std::string file_path = fmt::format("/{}/{}/{}/{}/{}",
//                                        scene, md5_prefix1, md5_prefix2, file_md5, clean_name);
//     // 物理路径拼接（修复substr依赖，用filesystem拼接更安全）

//     // 物理路径 = 根目录 + 规范path（如 /data/fastdfs/store/default/a1/b2/a1b2xxxx/test.jpg）
//     std::filesystem::path abs_phys_path = std::filesystem::path(config_.root_dir) / std::filesystem::path(file_path).relative_path();

  
    
//     // 写盘（带回滚）
//     bool write_ok = write_disk(abs_phys_path.string(), data);
//     if (!write_ok) {
//         LOG_ERROR << "Write disk failed: " << abs_phys_path.string();
//         // 回滚：删除残留目录/文件
//         std::filesystem::remove(abs_phys_path);
//         std::filesystem::remove(abs_phys_path.parent_path());
//         throw std::runtime_error("[FileStore] Write disk failed: " + abs_phys_path.string());
//     }
//     // 构造新的元数据
//     MetaFile new_meta;
//     new_meta.name = clean_name;
//     new_meta.scene = scene;
//     new_meta.md5 = file_md5;
//     new_meta.size = data.size();
//     new_meta.path = file_path;
//     new_meta.physical_path = abs_phys_path.string();
//     new_meta.mtime = std::time(nullptr);
//     new_meta.peers.push_back(config_.peer_id);
//     new_meta.url = fmt::format("{}{}", config_.domain, file_path); // 含 scene 的 URL


//     // 保存新元数据
//     if (!metadata_store->save_meta(new_meta)) {
//         throw std::runtime_error("[FileStore] Save meta failed (new file)");
//     }

//     return {new_meta, true}; // true表示新写入数据

// }



// bool FileStore::is_file_exist(const std::string &file_id)
// {
//     auto meta = metadata_store->get_meta(file_id);
//     if (meta.has_value() && meta.value().status == FileStatus::NORMAL) {
//         return true;
//     }
//     return false;
// }



std::vector<std::string>& FileStore::list_files()
{
    return config_.allow_extensions;
}

const std::string & FileStore::get_root_dir() const
{
    return config_.root_dir;
}

//扫目录获取分片文件
std::vector<int> FileStore::get_shard_index(const std::string &file_dir){

    std::vector<int> shard_index;
    std::filesystem::path file_path(file_dir);
    if (std::filesystem::exists(file_path) && std::filesystem::is_directory(file_path)) {
        for (const auto& entry : std::filesystem::directory_iterator(file_path)) {
            if (entry.is_regular_file()) {
                // 文件名就是 chunk index (例如 "0", "1")
                std::string filename = entry.path().filename().string();
               // 简单的防呆检查：确保文件名是数字
                try {
                    int index = std::stoi(filename);
                    
                    // 【进阶健壮性检查】：
                    // 最好检查一下文件大小是否等于标准分片大小 (例如 5MB)
                    // 如果大小不对（比如上次传一半断了），就不要加入列表，让前端重传
                    // if (entry.file_size() == EXPECTED_CHUNK_SIZE) { ... }

                    shard_index.push_back(index);
                } catch (...) {
                   LOG_ERROR << "Invalid shard file name: " << filename;
                }
            }
        }
    }
    return shard_index;
}

std::string FileStore::get_temp_dir() const
{
    return config_.temp_dir;
}

bool FileStore::save_file(const std::string_view &data, const std::string &rel_path)
{
    //安全检查
     if (rel_path.find("..") != std::string::npos) {
        LOG_ERROR << "Security warning: Illegal path traversal attempt: " << rel_path;
        return false;
    }
    // 1. 路径拼接（保持封装性）
    // 使用 config_.root_dir 锚定根目录
    std::filesystem::path full_path = std::filesystem::path(config_.root_dir) / rel_path;

    // 2. [关键] 自动创建父目录
    // 如果目录已存在，create_directories 啥也不做，开销很小
    std::error_code ec;
    std::filesystem::create_directories(full_path.parent_path(), ec);
    if (ec) {
        LOG_ERROR << "Create directory failed: " << full_path.parent_path() << " Error: " << ec.message();
        return false;
    }

    // 3. 写入文件
    // 假设 write_disk 内部封装了 ofstream
    if (!write_disk(full_path.string(), data)) {
        LOG_ERROR << "Write disk failed: " << full_path.string();
        
        // 4. [修正] 只回滚当前文件，绝对不要删 parent_path
        std::filesystem::remove(full_path, ec); 
        
        return false; // 既然返回 bool，就不要 throw，交给上层处理
    }

    return true;
}

std::optional<std::string> FileStore::save_file(const std::string& filename, const std::string_view &data, const std::string &scene, 
        const std::string& md5)
{
    //计算md5
    std::string file_md5 = md5.empty() ? CryptoUtils::md5(data) : md5;
    std::string base_rel_path = CryptoUtils::md5_to_rel_path(file_md5);

   // 2. 提取后缀名 (例如 ".jpg" 或 "")
    // std::filesystem::path 能够智能处理文件名
    std::string ext = std::filesystem::path(filename).extension().string();

        // 4. [关键] 拼接后缀！
    // 最终 rel_path_str 变成: "e1/0a/md5字符串.jpg"
    std::string rel_path_str = base_rel_path + ext; 

    //计算保存地址
    std::filesystem::path abs_root(config_.root_dir);
    std::filesystem::path abs_file_path = abs_root / scene /rel_path_str;

    LOG_INFO << "Save file to: " << abs_file_path.string();

    if (std::filesystem::exists(abs_file_path)) {
        LOG_INFO << "File dedup hit: " << rel_path_str;
        return (std::filesystem::path(scene) / rel_path_str).string(); 
    }

    // 6. 创建父目录 (必须做，否则 ofstream 会失败)
    std::error_code ec;
    std::filesystem::create_directories(abs_file_path.parent_path(), ec);
    if (ec) {
        LOG_ERROR << "Create dir failed: " << ec.message();
        return std::nullopt;
    }

        // 7. 写入磁盘 (复用你之前写好的 write_disk 逻辑)
    if (!write_disk(abs_file_path.string(), data)) {
        return std::nullopt;
    }

    return (std::filesystem::path(scene) / rel_path_str).string();

}

bool FileStore::save_shard(const std::string file_md5, uint32_t shard_index, const std::string &content,
                           const std::string &shard_md5)
{
    if (CryptoUtils::md5(content) != shard_md5) {
        LOG_ERROR << "Shard File MD5 verify fail" << shard_md5;
        return false;
    }

    std::string rel_path = config_.root_dir + "/" + config_.temp_dir + "/" + file_md5 + "/" + file_md5 + "_" + std::to_string(shard_index);

    //写入磁盘
    bool write_ok = write_disk(rel_path, content);
    if (!write_ok) {
        LOG_ERROR << "write into file fail";
        return false;
    }

    // 写入一条数据到leveldb用于断点续传
    //metadata_store->save_meta_shard(shard_index, file_md5, shard_md5);

    return true;
}

std::optional<std::string> FileStore::get_file(const std::string & path){
    // 1. 路径拼接（保持封装性）
    std::filesystem::path full_path = std::filesystem::path(config_.root_dir) / path;
    if (!std::filesystem::exists(full_path)) {
        LOG_ERROR << "File not exist: " << full_path.string();
        return std::nullopt;
    }
    if (!std::filesystem::is_regular_file(full_path)) { // 排除目录/符号链接
        LOG_ERROR << "Not a regular file: " << full_path.string();
        return std::nullopt;
    }
   
    std::string content;
    if (read_disk(full_path.string(), content)) {
        return content;
    }
    return std::nullopt;
}

// std::optional<std::string> FileStore::get_file(const std::string &file_md5)
// {

//     // ========== 1. 查元数据 + 基础校验 ==========
//     auto meta_opt = metadata_store->get_meta(file_md5);
//     if (!meta_opt.has_value()) {
//         LOG_WARN << "Meta not found for MD5: " << file_md5;
//         return std::nullopt;
//     }
//     const auto& meta = meta_opt.value();
//       // ========== 2. 安全拼接物理路径（修复substr风险） ==========
//     std::filesystem::path path_meta(meta.path);
//     // 无论 path 是否以 / 开头，都取「相对路径」拼接（避免 substr 截取错误）
//     std::filesystem::path abs_phys_path = std::filesystem::path(meta.physical_path) / path_meta.relative_path();
//     std::string abs_path_str = abs_phys_path.string();

//     // ========== 3. 检查文件是否存在（提前拦截） ==========
//     if (!std::filesystem::exists(abs_phys_path)) {
//         LOG_ERROR << "File not exist: " << abs_path_str;
//         return std::nullopt;
//     }
//     if (!std::filesystem::is_regular_file(abs_phys_path)) { // 排除目录/符号链接
//         LOG_ERROR << "Not a regular file: " << abs_path_str;
//         return std::nullopt;
//     }

//     // ========== 4. 打开文件 + 异常安全读取 ==========
//      std::ifstream file(abs_path_str, std::ios::binary);
//     if (!file.is_open()) {
//         LOG_ERROR << "Open file failed (errno: " << errno << "): " << abs_path_str;
//         return std::nullopt;
//     }
//   // 局部缓冲区版本（更安全）
//     std::string local_buffer; // 局部变量，线程安全
//     uint64_t file_size = std::filesystem::file_size(abs_phys_path);
//     try {
//         local_buffer.reserve(file_size);
//         char read_buffer[READ_BUFFER_SIZE];
//         while (file.read(read_buffer, READ_BUFFER_SIZE)) {
//             local_buffer.append(read_buffer, file.gcount());
//         }
//         size_t last_bytes = file.gcount();
//         if (last_bytes > 0) {
//             local_buffer.append(read_buffer, last_bytes);
//         }
//         return local_buffer; // 返回拷贝（RVO优化，无性能损失）
//     }catch (const std::exception& e) {
//         LOG_ERROR << "Exception during file read: " << e.what();
//         return std::nullopt;
//     }
//     return local_buffer;
// }

// // 分片上传逻辑
// std::pair<MetaFile, bool> FileStore::save_file_shard(const std::string &filename, const std::string &data, 
//     const std::string &file_md5, const std::string &scene, uint32_t shard_count, uint64_t shard_std_size, 
//         uint32_t shard_index,const std::string& shard_md5_ ) {
//         //参数校验
//     if (!CryptoUtils::check_scene(scene, config_.allow_scenes)) {
//         LOG_ERROR << "Scene not allowed: " << scene;
//         throw std::invalid_argument("[FileStore] Invalid scene: " + scene);
//     }
    
//     std::string clean_name = filename;
//     if (config_.enable_trim_filename) {
//         clean_name = CryptoUtils::trim_filename_special_char(filename);
//     }

    

//     if (CryptoUtils::check_file_ext(filename, config_.allow_extensions) == false) {
//         LOG_ERROR << "File extension not allowed: " << filename;
//         throw std::invalid_argument("[FileStore] Invalid scene: " + scene);
//     }

//     if (clean_name.empty()) {
//         LOG_ERROR << "Filename is empty after trimming special characters.";
//         throw std::invalid_argument("[FileStore] Filename is empty after trimming special characters.");
//     }

//     //计算md5
//     std::string shard_md5 = CryptoUtils::md5(data);
//     if (file_md5.empty()) {
//         throw std::runtime_error("[FileStore] MD5 calculation failed");
//     }

//     if (shard_md5_ != "" && shard_md5_ != shard_md5) {
//         LOG_ERROR << "Provided MD5 does not match calculated MD5.";
//         throw std::invalid_argument("[FileStore] Provided MD5 does not match calculated MD5.");
//     }


//      //去重检查,秒传
//     auto existing_meta = metadata_store->get_meta(shard_md5);
//     if (config_.enable_distinct_file && existing_meta.has_value()) {
//         LOG_INFO << "File already exists, deduplication enabled. MD5: " << file_md5;
        
//         //构造新的元数据，更新逻辑路径/文件名/修改时间
//         MetaFile new_meta = existing_meta.value();
//         new_meta.name = clean_name;
//         new_meta.scene = scene;
//         new_meta.mtime = std::time(nullptr);
    
//         new_meta.path = fmt::format("/{}/{}", file_md5, file_md5 + "_" + std::to_string(shard_index));
    
//         auto last = std::unique(new_meta.peers.begin(), new_meta.peers.end());
//         new_meta.peers.erase(last, new_meta.peers.end());
        

//         // 保存更新后的元数据
//         if (!metadata_store->save_meta(new_meta)) {
//             throw std::runtime_error("[FileStore] Save meta failed (deduplication)");
//         }

//         return {new_meta, false}; // false表示未新写入数据
//     }

//     std::string file_path = fmt::format("/{}/{}", file_md5, file_md5 + "_" + std::to_string(shard_index));
//     // 物理路径 = 根目录 + 规范path（如 /data/fastdfs/store/default/a1/b2/a1b2xxxx/test.jpg）
//     std::filesystem::path abs_phys_path = std::filesystem::path(config_.temp_dir) / std::filesystem::path(file_path).relative_path();

//      // 确保目录存在
//     if (!std::filesystem::exists(abs_phys_path.parent_path())) {
//         std::filesystem::create_directories(abs_phys_path.parent_path());
//     }
//     //新文件逻辑
//     //4、计算物理存储地址

//     // 写盘（带回滚）
//     bool write_ok = write_disk(abs_phys_path.string(), data);
//     if (!write_ok) {
//         LOG_ERROR << "Write disk failed: " << abs_phys_path.string();
//         // 回滚：删除残留目录/文件
//         std::filesystem::remove(abs_phys_path);
//         std::filesystem::remove(abs_phys_path.parent_path());
//         throw std::runtime_error("[FileStore] Write disk failed: " + abs_phys_path.string());
//     }

//     MetaFile new_meta;
    
//     new_meta.path = fmt::format("/{}/{}", file_md5, file_md5 + "_" + std::to_string(shard_index));
//     new_meta.name = clean_name;
//     new_meta.scene = scene;
//     new_meta.mtime = std::time(nullptr);
//     new_meta.is_sharded = true;
//     new_meta.shard_count = shard_count;
//     new_meta.shard_std_size = shard_std_size;
//     FileShard shard;
//     shard.index = shard_index;
//     shard.size = data.size();
//     shard.md5 = shard_md5;
//     shard.shard_id = file_md5 + "_" + std::to_string(shard_index);
//     new_meta.shards.push_back(shard);
    
//     new_meta.physical_path = fmt::format("{}/{}", config_.temp_dir, new_meta.path);

//     auto last = std::unique(new_meta.peers.begin(), new_meta.peers.end());
//     new_meta.peers.erase(last, new_meta.peers.end());

//     // 保存新元数据
//     if (!metadata_store->save_meta_shard(new_meta, shard_index, file_md5)) {
//         throw std::runtime_error("[FileStore] Save meta failed (new shard file)");
//     }

//     return {new_meta, true}; // false表示未新写入数据

// }
}
