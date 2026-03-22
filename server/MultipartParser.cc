

#include "MultipartParser.h"
#include "common/logger.h" // 假设你有日志库
#include "HttpContext.h"
namespace cdfs {

// 辅助函数：提取 key="value" 中的 value
std::string MultipartParser::extract_value(std::string_view header, std::string_view key) {
    size_t pos = header.find(key);
    if (pos == std::string_view::npos) return "";

    // key 之后应该是 =
    size_t start = pos + key.length();
    if (start >= header.length() || header[start] != '=') return "";
    start++; // 跳过 '='

    // 情况 A: 值被引号包围 name="value"
    if (start < header.length() && header[start] == '"') {
        start++; // 跳过左引号
        size_t end = header.find('"', start);
        if (end == std::string_view::npos) return "";
        return std::string(header.substr(start, end - start));
    } 
    // 情况 B: 值没有引号 (虽然不规范，但要兼容)
    else {
        size_t end = header.find_first_of("; \r\n", start);
        if (end == std::string_view::npos) end = header.length();
        return std::string(header.substr(start, end - start));
    }
}

std::optional<MultipartForm> MultipartParser::parse(const HttpContext& ctx) {
    const auto& req = ctx.req;
    
    // 1. 检查 Content-Type
    std::string_view content_type = req[boost::beast::http::field::content_type];
    std::string boundary_prefix = "boundary=";
    size_t pos_boundary = content_type.find(boundary_prefix);
    
    if (pos_boundary == std::string_view::npos) {
        // LOG_ERROR << "MultipartParser: No boundary found";
        return std::nullopt;
    }

    // 2. 提取 Boundary 字符串
    std::string_view boundary = content_type.substr(pos_boundary + boundary_prefix.length());
    // 有些浏览器发的 boundary 可能带引号，去掉它
    if (boundary.size() >= 2 && boundary.front() == '"' && boundary.back() == '"') {
        boundary = boundary.substr(1, boundary.size() - 2);
    }

    // 构造分隔符
    // Body 中的分隔符是 "--" + boundary
    std::string delimiter = "--" + std::string(boundary);
    std::string end_delimiter = delimiter + "--";

    std::string_view body = req.body();
    MultipartForm form;

    // 3. 循环解析所有分片
    size_t pos = 0;
    while (true) {
        // 查找下一个分隔符
        size_t part_start = body.find(delimiter, pos);
        if (part_start == std::string_view::npos) break; // 找不到了，结束

        // 检查是否到达整个 Body 的末尾 (--boundary--)
        // 需要检查长度防止越界
        if (part_start + end_delimiter.length() <= body.length()) {
            if (body.substr(part_start, end_delimiter.length()) == end_delimiter) {
                break; // 正常结束
            }
        }

        // 此时 part_start 指向 delimiter 的开头
        // 真正的 Header 开始位置是 delimiter 后面紧跟的 \r\n
        size_t header_start = part_start + delimiter.length();
        
        // 容错处理：理论上必须跟 \r\n
        if (header_start + 2 > body.length()) break;
        if (body[header_start] == '\r' && body[header_start+1] == '\n') {
            header_start += 2;
        } else {
            // 格式异常，尝试跳过继续找
            pos = header_start;
            continue;
        }

        // 查找 Header 与 Body 的分隔符 (\r\n\r\n)
        size_t header_end = body.find("\r\n\r\n", header_start);
        if (header_end == std::string_view::npos) break; // 格式错误

        // 提取 Header 区域
        std::string_view header_part = body.substr(header_start, header_end - header_start);

        // 解析 Header 里的元信息
        std::string name = extract_value(header_part, "name");
        std::string filename = extract_value(header_part, "filename");

        // 计算数据的开始和结束
        size_t data_start = header_end + 4; // 跳过 \r\n\r\n
        size_t next_boundary = body.find(delimiter, data_start);
        
        if (next_boundary == std::string_view::npos) break; // 数据被截断了

        size_t data_end = next_boundary;
        // 关键点：HTTP 协议规定，数据最后会有一个 \r\n，不属于数据本身
        if (data_end >= 2 && body[data_end-2] == '\r' && body[data_end-1] == '\n') {
            data_end -= 2;
        }

        // 提取数据内容
        std::string_view data_view = body.substr(data_start, data_end - data_start);

        // 4. 保存结果
        if (!filename.empty()) {
            // Case A: 这是一个文件
            form.filename = filename;
            form.content = data_view;
            // 这里可以解析 Content-Type 行，如果需要的话
        } else if (!name.empty()) {
            // Case B: 这是一个普通字段 (scene, path, etc.)
            form.params[name] = std::string(data_view);
        }

        // 更新循环位置
        pos = next_boundary;
    }

    return form;
}
}