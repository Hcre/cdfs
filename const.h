#ifndef __CONST_H__
#define __CONST_H__

#include <boost/beast/core/error.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <unordered_map>
#include <functional>
#include <string>
#include <utility>
#include <string_view>
#include <thread>
#include <mutex>
#include <sstream>
#include <filesystem>
#include <variant>
#include <map>

namespace beast = boost::beast;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;
namespace http = beast::http;


// 为了方便，给 Beast 类型起个别名
using StringRequest = http::request<http::string_body>;
using StringResponse = http::response<http::string_body>;
using ResponseSender = std::function<void(http::message_generator)>;

namespace cdfs {
    // 定义全局变量 (在 main.cpp 或某个 .cpp 顶部)
// 记录服务器启动时间点
extern std::chrono::steady_clock::time_point g_start_time;
// 记录处理的总请求数 (原子操作，线程安全)
extern std::atomic<uint64_t> g_total_requests;

// 【新增】流量统计
extern std::atomic<uint64_t> g_bytes_sent;     // 发送总字节数
extern std::atomic<uint64_t> g_bytes_received; // 接收总字节数

// 【新增】并发连接数
extern std::atomic<int32_t> g_active_connections;

extern std::atomic<uint64_t> g_total_latency_us; // 总耗时 (微秒)
extern std::atomic<int>      g_max_latency_us;   // 周期内最大耗时 (微秒)

}



#endif