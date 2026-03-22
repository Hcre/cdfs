#include "FileCtl.h"
#include "file/CryptoUtils.h"
#include "common/MineMap.h"

namespace cdfs {

FileCtl::FileCtl(std::shared_ptr<FileStore> fileStore, std::shared_ptr<MetadataStore> metaStore)
    :fileStore_(fileStore), metadataStore_(metaStore) {

}

//
void FileCtl::handleCheck(const HttpContext &ctx, ResponseSender sender) {
    auto file_md5_it = ctx.query.find("md5");
    if (file_md5_it == ctx.query.end() || file_md5_it->second.empty()) {   
        
        sender(ctx.error("md5 is empty", http::status::bad_request));
        return;
    }

    const std::string& file_md5 = file_md5_it->second;
    bool exist = metadataStore_->exists(file_md5);
    if (exist) {
        sender(ctx.success(nlohmann::json::object(), "file exist"));
    } else {
        sender(ctx.success(nlohmann::json::object(), "file not exist"));
    }
   
}

void FileCtl::handleUpload(const HttpContext &ctx, ResponseSender sender){

    int current = concurrent_uploads_.fetch_add(1);
    if (current >= MAX_CONCURRENT_UPLOADS) {
        concurrent_uploads_.fetch_sub(1); // 恢复计数
        return sender(ctx.error("Server busy, too many uploads"));
    }
    //如果带了md5，说明客户端想尝试秒传
    auto file_it = ctx.query.find("md5");
    if (file_it != ctx.query.end() && !file_it->second.empty())  {
        const std::string& file_md5 = file_it->second;
        auto meta = metadataStore_->get_meta(file_md5);
        if (meta.has_value()) {          
            sender(ctx.success(*meta, "Deduplication success"));
            return;   
        }
    }   

    if (!ctx.is_multipart()) {
        return sender(ctx.error("Request is not a multipart/form-data"));
    }

    // 2. 获取表单 (内部自动调用 MultipartParser 解析)
    const auto& form = ctx.get_form();

      // 3. 校验
    if (form.content.empty()) {
        return sender(ctx.error("File content is empty"));
    }

    std::string filename = form.filename;
    std::string_view content = form.content;
    std::string scene = form.get_param("scene", "default");
    std::string path = form.get_param("path");

    LOG_INFO << "Upload file: " << filename << " size: " << content.size() << " scene: " << scene << " path: " << path;
    
    //文件名单检查
    if (CryptoUtils::check_file_ext(filename, fileStore_->list_files()) == false) {
        LOG_ERROR << "File extension not allowed: " << filename;
        sender(ctx.error("File extension not allowed"));
        return;
    }
    
    //文件名清理
    std::string clean_name = filename;
    if (metadataStore_->is_enable_trim_filename()) {
        clean_name = CryptoUtils::trim_filename_special_char(filename);
    }

    //场景检查

    try {

        std::string file_md5 = CryptoUtils::md5(content);
        std::optional<std::string> file_path = fileStore_->save_file(filename,content, scene, file_md5);

        if (clean_name.empty()) {
            LOG_ERROR << "Filename is empty after trimming special characters.";
            throw std::invalid_argument("[FileStore] Filename is empty after trimming special characters.");
        }

        //保存元数据
        MetaFile new_meta;
        new_meta.name = clean_name;
        new_meta.scene = scene;
        new_meta.md5 = file_md5;
        new_meta.size = content.size();
        new_meta.path = file_path.value();
        new_meta.mtime = std::time(nullptr);
        new_meta.peers.push_back(metadataStore_->get_peer_id());

        metadataStore_->save_meta(new_meta);
        LOG_INFO << "Upload success: " << new_meta.path;


        //暂时不写group
        // std::string full_url = metadataStore_->get_domain() + "/" + config_->group_name + "/" + new_meta.path;
        std::string full_url = metadataStore_->get_domain() + "/" + new_meta.path;

        // 2. 组装 data 对象 (对应你 JSON 里的 "data" 部分)
        nlohmann::json res_data = {
            {"url",   full_url},       // 只有这里需要计算
            {"path",  new_meta.path},  // 直接取自 meta
            {"md5",   new_meta.md5},
            {"scene", new_meta.scene},
            {"size",  new_meta.size},
            {"mtime", new_meta.mtime},
            {"name",  new_meta.name}   // 原始文件名
        };

        sender(ctx.success(res_data, "Upload success"));

    }catch (const std::exception& e) {
        sender(ctx.error(e.what()));
    }

}
void FileCtl::handleDownload(const HttpContext & ctx, ResponseSender sender) {
    // 1. [性能优化] 压测时务必注释掉日志！
    // LOG_INFO << "Download file: " << ctx.path_suffix;

    std::string rel_path = ctx.path_suffix;
    if (rel_path.empty()) {
        return sender(ctx.not_found("File path missing"));
    }

    // 2. 计算物理绝对路径 (这是用来读磁盘的，绝对不能改！)
    // 假设 fileStore_->get_root_dir() 返回的是 string，最好确保它末尾没斜杠，或者用 filesystem 拼接
    std::filesystem::path abs_path = std::filesystem::path(fileStore_->get_root_dir() + rel_path);
    
    // 3. 准备 HTTP Body
    http::file_body::value_type body;
    boost::system::error_code ec;

    // [性能优化] 直接 open，不要先调 fs::exists (减少一次系统调用)
    body.open(abs_path.c_str(), boost::beast::file_mode::scan, ec);

    if (ec == boost::system::errc::no_such_file_or_directory) {
        return sender(ctx.not_found("File not found"));
    } else if (ec) {
        return sender(ctx.error("File open error"));
    }

    auto const file_size = body.size();

    // 4. 构造响应
    http::response<http::file_body> res{
        std::piecewise_construct,
        std::make_tuple(std::move(body)),
        std::make_tuple(http::status::ok, ctx.req.version())
    };

    res.set(http::field::server, "MyDFS");
    res.content_length(file_size);
    
    // [重要] 设置 Content-Type，否则图片无法在浏览器预览
    // 简单的 MIME 判断 (建议封装到 MimeTypes::get)
    // res.set(http::field::content_type, MimeTypes::get(abs_path.string()));
    // 这里暂时给个通用的，最好根据后缀判断
    res.set(http::field::content_type, MimeMap::get_mime_type(abs_path));

    // 5. 处理重命名逻辑 (只改 Header，不改物理路径)
    auto rename_it = ctx.query.find("attname");
    if (rename_it != ctx.query.end() && !rename_it->second.empty()) {
        // 用户指定了下载名 -> 强制下载
        std::string new_filename = rename_it->second;
        // 加上双引号防止文件名带空格出错
        res.set(http::field::content_disposition, "attachment; filename=\"" + new_filename + "\"");
    } else {
        // 用户没指定 -> 默认在线预览 (Inline)
        // 这样浏览器会尝试显示图片，而不是直接下载
        res.set(http::field::content_disposition, "inline");
    }
    
    // 6. 发送
    // 显式构造 message_generator
    sender(http::message_generator(std::move(res)));
}

void FileCtl::handleCheckChunk(const HttpContext &ctx, ResponseSender sender)
{
    std::string file_md5;
    auto file_it = ctx.query.find("md5");
    if (file_it != ctx.query.end() && !file_it->second.empty())  {
        file_md5 = file_it->second;
    
    }   

    if (file_md5.empty()) {
        return sender(ctx.error("md5 is empty"));
    }

    //拼接地址
    std::string file_path = fileStore_->get_root_dir()+ "/" + fileStore_->get_temp_dir() + "/" + file_md5;
    std::vector<int> up_loaded_chunks = fileStore_->get_shard_index(file_path);
    nlohmann::json res_data = {
        {"list", up_loaded_chunks}
    };
    sender(ctx.success(res_data));
}

void FileCtl::handleChunkUpload(const HttpContext &ctx, ResponseSender sender)
{
    // 1. 获取表单 (内部自动调用 MultipartParser 解析)
    const auto& form = ctx.get_form();

    std::string file_md5 = form.get_param("md5");
    std::string_view content = form.content;
    std::string chunk_index = form.get_param("chunk");

    if (file_md5.empty() || content.empty() || chunk_index.empty()) {
        return sender(ctx.error("md5 or content or chunk is empty"));
    }

    //拼接地址
    std::string file_path = fileStore_->get_temp_dir() + "/" + file_md5 + "/" + chunk_index;
    LOG_INFO << "Upload chunk: " << file_path;
    //保存文件
    bool ret = fileStore_->save_file(content, file_path);
 
    if (ret) {
        nlohmann::json res_data = {
            {"chunk", chunk_index}
        };
        sender(ctx.success(res_data));
        return;
    }
    sender(ctx.error("Save file failed"));
}

void FileCtl::handleMerge(const HttpContext &ctx, ResponseSender sender)
{
    // 1. 获取表单参数
    const auto& form = ctx.get_form();
    std::string file_md5 = form.get_param("md5");
    std::string filename = form.get_param("filename");
    std::string scene = form.get_param("scene", "default");

    if (file_md5.empty() || filename.empty()) {
        return sender(ctx.error("md5 or filename is empty"));
    }

    // 2. 定位临时目录
    // 路径格式: /temp_root/md5_value/
    std::filesystem::path temp_chunk_dir = std::filesystem::path(fileStore_->get_root_dir() + "/ "+ fileStore_->get_temp_dir()) / file_md5;

    if (!std::filesystem::exists(temp_chunk_dir)) {
        return sender(ctx.error("Chunk directory not found or expired"));
    }

    try {
        // 3. 收集并排序分片索引
        // 必须按数字顺序合并 (0, 1, 2...), 不能按字符串顺序 (1, 10, 2...)
        std::vector<int> chunk_indices;
        for (const auto& entry : std::filesystem::directory_iterator(temp_chunk_dir)) {
            if (entry.is_regular_file()) {
                try {
                    // 假设分片文件名就是索引数字 "0", "1"
                    int idx = std::stoi(entry.path().filename().string());
                    chunk_indices.push_back(idx);
                } catch (...) {
                    continue; // 忽略非数字文件
                }
            }
        }
        
        if (chunk_indices.empty()) {
            return sender(ctx.error("No chunks found"));
        }

        // 升序排序
        std::sort(chunk_indices.begin(), chunk_indices.end());

        // 4. 执行合并 (Merge)
        // 先合并到一个临时的大文件里，比如 /temp_root/md5_value.merged
        std::filesystem::path merged_temp_file = temp_chunk_dir.parent_path() / (file_md5 + ".merged");
        
        {
            std::ofstream out_file(merged_temp_file, std::ios::binary | std::ios::trunc);
            if (!out_file.is_open()) {
                throw std::runtime_error("Cannot create merge output file");
            }

            // 依次读取分片并写入
            for (int idx : chunk_indices) {
                std::filesystem::path chunk_path = temp_chunk_dir / std::to_string(idx);
                std::ifstream in_file(chunk_path, std::ios::binary);
                if (!in_file.is_open()) {
                    throw std::runtime_error("Cannot open chunk: " + std::to_string(idx));
                }
                // 使用 rdbuf 高效传输流
                out_file << in_file.rdbuf();
            }
            out_file.close();
        }

        // 5. 生成正式存储路径
        // 逻辑: scene + md5打散路径 + 后缀
        std::string ext = std::filesystem::path(filename).extension().string();
        std::string base_rel_path = CryptoUtils::md5_to_rel_path(file_md5); // e1/0a/md5
        std::string final_rel_path = base_rel_path + ext; // e1/0a/md5.mp4
        
        // 物理路径
        std::filesystem::path abs_root(fileStore_->get_root_dir());
        std::filesystem::path final_abs_path = abs_root / scene / final_rel_path;

        // 确保目标父目录存在
        std::filesystem::create_directories(final_abs_path.parent_path());

        // 6. 原子移动 (Rename)
        // 将合并好的临时文件直接移动到正式位置
        std::filesystem::rename(merged_temp_file, final_abs_path);

        // 7. 清理临时分片目录
        // 放在最后，确保前面成功了再删
        std::filesystem::remove_all(temp_chunk_dir);

        // 8. 保存元数据
        MetaFile new_meta;
        new_meta.name = filename; // 原始文件名
        new_meta.scene = scene;
        new_meta.md5 = file_md5;
        new_meta.size = std::filesystem::file_size(final_abs_path);
        new_meta.path = (std::filesystem::path(scene) / final_rel_path).string(); // 存相对路径
        new_meta.mtime = std::time(nullptr);
        new_meta.peers.push_back(metadataStore_->get_peer_id());

        metadataStore_->save_meta(new_meta);
        
        LOG_INFO << "Merge success: " << new_meta.path << " size: " << new_meta.size;

        // 9. 返回成功响应
        std::string full_url = metadataStore_->get_domain() + "/" + new_meta.path; // 假设 config 暂时没法直接访问，用 metaStore 的 helper
        
        nlohmann::json res_data = {
            {"url",   full_url},
            {"path",  new_meta.path},
            {"md5",   new_meta.md5},
            {"scene", new_meta.scene},
            {"size",  new_meta.size},
            {"mtime", new_meta.mtime},
            {"name",  new_meta.name}
        };

        sender(ctx.success(res_data, "Merge success"));

    } catch (const std::exception& e) {
        LOG_ERROR << "Merge failed: " << e.what();
        // 尝试清理可能产生的中间文件
        // std::filesystem::remove(merged_temp_file); 
        sender(ctx.error(std::string("Merge failed: ") + e.what()));
    }
}
}