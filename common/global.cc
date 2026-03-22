#ifndef __COMMON_GLOBAL_H__
#define __COMMON_GLOBAL_H__

#include "const.h"

namespace cdfs {
    std::chrono::steady_clock::time_point g_start_time;
    std::atomic<uint64_t> g_total_requests{0};
    // 【新增】流量统计
    std::atomic<uint64_t> g_bytes_sent{0};     // 发送总字节数
    std::atomic<uint64_t> g_bytes_received{0}; // 接收总字节数

    // 【新增】并发连接数
    std::atomic<int32_t> g_active_connections{0};


    std::atomic<uint64_t> g_total_latency_us{0};
    std::atomic<int>      g_max_latency_us{0};
    
}

#endif // __COMMON_GLOBAL_H__