#ifndef __HTTP_CONTEXT_H__
#define __HTTP_CONTEXT_H__
#include "const.h"
#include <nlohmann/json.hpp>
#include "common/logger.h"
#include "MultipartParser.h"

namespace cdfs {

// 为了方便，给 Beast 类型起个别名


// --- 核心结构体 ---
struct HttpContext {
    const StringRequest& req;           // 原始请求
    std::string method;                 // 请求方法
    std::string path;                   // 纯净路径
    std::map<std::string, std::string> query; // 查询参数
    std::string path_suffix;            // 路径后缀参数

     // 缓存一下解析结果，避免重复解析
    mutable std::optional<MultipartForm> cached_form_;


    // http::verb 转字符串（用于日志/调试）

    StringResponse make_response(std::string_view content, std::string content_type, http::status status) const {
        StringResponse res{status, req.version()};
        res.set(http::field::server, "CDFS/1.0");
        res.set(http::field::content_type, content_type);
        res.keep_alive(req.keep_alive());
        res.body() = std::string(content);
        res.prepare_payload();
        // 1. 允许所有源（测试阶段用，生产可限定为你的前端地址）
        res.set(http::field::access_control_allow_origin, "*");
    
        // 2. 允许的方法 (通常下载是 GET，上传是 POST，预检是 OPTIONS)
        // 注意：你之前漏了 GET，下载必须允许 GET
        res.set(http::field::access_control_allow_methods, "GET, POST, OPTIONS, PUT, DELETE");
        
        // 3. 允许的头
        res.set(http::field::access_control_allow_headers, "*");

        // 4. (可选) 允许前端获取特定的 header (比如Content-Disposition用于获取文件名)
        res.set(http::field::access_control_expose_headers, "Content-Disposition");

        return res; // <--- 这里返回对象，而不是 sender(res)
    }


      // ==========================================
    // 辅助方法：返回特定类型的响应对象
    // ==========================================

    StringResponse text(std::string_view content, http::status status = http::status::ok) const {
        return make_response(content, "text/plain; charset=utf-8", status);
    }

    StringResponse html(std::string_view content, http::status status = http::status::ok) const {
        return make_response(content, "text/html; charset=utf-8", status);
    }

    StringResponse json(const nlohmann::json& data, http::status status = http::status::ok) const {
        return make_response(data.dump(), "application/json", status);
    }

    // 标准成功响应
    StringResponse success(const nlohmann::json& data = nlohmann::json::object(), const std::string& msg = "") const {
        nlohmann::json j = {
            {"status", "ok"},
            {"message", msg},
            {"data", data}
        };
        // 调用底层的 make_response，自动带上 CORS
        return make_response(j.dump(), "application/json", http::status::ok);
    }

    // 3. 失败的 JSON 响应
    StringResponse error(const std::string& msg, http::status status = http::status::bad_request) const {
        nlohmann::json j = {
            {"status", "fail"},
            {"message", msg},
            {"data", nullptr}
        };
        // 调用底层的 make_response，自动带上 CORS
        return make_response(j.dump(), "application/json", status);
    }
    
    // 404 便捷方法
    StringResponse not_found(const std::string& msg = "Not Found") const {
        return error(msg, http::status::not_found);
    }

  // 提供一个成员函数
    const MultipartForm& get_form() const {
        if (!cached_form_) {
            // 惰性加载：只有第一次调用时才解析
            cached_form_ = MultipartParser::parse(*this);
            if (!cached_form_) {
                // 如果解析失败，给一个空对象或者抛出异常，视策略而定
                static MultipartForm empty;
                return empty;
            }
        }
        return cached_form_.value();
    }
    
    // 辅助判断是否是 multipart 请求
    bool is_multipart() const {
        auto ct = req[http::field::content_type];
        return ct.find("multipart/form-data") != std::string::npos;
    }


    static bool parse_multipart(const HttpContext& ctx, std::string& out_filename, std::string& out_content) {
    const auto& req = ctx.req;
    std::string_view body = req.body();
    std::string_view content_type = req[http::field::content_type];

    // 1. 提取 Boundary
    // 格式通常是: multipart/form-data; boundary=----WebKitFormBoundary...
    std::string boundary_prefix = "boundary=";
    auto pos_boundary = content_type.find(boundary_prefix);
    if (pos_boundary == std::string_view::npos) {
        LOG_ERROR << "No boundary found in Content-Type";
        return false;
    }

    // 获取 boundary 的值
    std::string_view boundary = content_type.substr(pos_boundary + boundary_prefix.length());
    
    // 实际上 Body 里的 boundary 前面会有两个横杠 "--"
    std::string delimiter = "--" + std::string(boundary);
    std::string end_delimiter = delimiter + "--"; // 整个请求结束的标志

    // 2. 查找第一个 Part 的开始
    // Body 通常以 delimiter 开头
    auto pos_start = body.find(delimiter);
    if (pos_start == std::string_view::npos) return false;

    // 此时 pos_start 指向第一个分片的开头。我们需要解析头部来找到 filename
    // 格式：
    // --boundary
    // Content-Disposition: form-data; name="file"; filename="a.jpg"\r\n
    // Content-Type: image/jpeg\r\n
    // \r\n
    // [Data]
    
    // 寻找头部结束的位置 (\r\n\r\n)
    auto pos_header_end = body.find("\r\n\r\n", pos_start);
    if (pos_header_end == std::string_view::npos) return false;

    // 3. 解析 Header (在 pos_start 到 pos_header_end 之间)
    std::string_view header_part = body.substr(pos_start, pos_header_end - pos_start);
    
    // 寻找 filename="
    std::string filename_key = "filename=\"";
    auto pos_filename = header_part.find(filename_key);
    if (pos_filename == std::string_view::npos) {
        LOG_ERROR << "No filename found in part header";
        return false; // 说明这可能不是文件字段，而是普通表单字段
    }

    // 提取 filename
    size_t filename_start = pos_filename + filename_key.length();
    size_t filename_end = header_part.find("\"", filename_start);
    if (filename_end == std::string_view::npos) return false;

    out_filename = std::string(header_part.substr(filename_start, filename_end - filename_start));

    // 4. 提取 Content
    // 数据开始位置：Header 结束 (\r\n\r\n) 之后，跳过这 4 个字符
    size_t data_start = pos_header_end + 4;

    // 数据结束位置：查找下一个 boundary
    // 注意：数据和下一个 boundary 之间有一个 \r\n
    auto pos_next_boundary = body.find(delimiter, data_start);
    if (pos_next_boundary == std::string_view::npos) {
        LOG_ERROR << "Unexpected end of body (no closing boundary)";
        return false;
    }

    // 真正的数据结束位置通常在 boundary 前面的 \r\n 处
    // 但为了保险，我们只截取到 pos_next_boundary，然后去掉尾部的 \r\n
    // 标准规定：part data 后面紧跟 CR LF，然后是 boundary
    size_t data_end = pos_next_boundary;
    
    // 回退两个字符去检查 \r\n
    if (data_end >= 2 && body[data_end-2] == '\r' && body[data_end-1] == '\n') {
        data_end -= 2;
    }

    // 5. 复制数据到输出
    // 注意：这里是二进制拷贝，std::string 是二进制安全的
    out_content = std::string(body.substr(data_start, data_end - data_start));

    return true;
}
};


// --- Handler 定义也放在这里 ---
// 因为 Handler 依赖 Context，放在一起最自然
using HandlerFunction = std::function<void(const HttpContext& ctx, ResponseSender sender)>;
}

#endif