#include <fstream>
#include <sstream>
#include <vector>
#include <unistd.h> // sysconf

namespace cdfs {

class SystemCtl {
private:
    // 上一次计算 CPU 时的快照
    struct CpuSnapshot {
        uint64_t total_time = 0; // 系统总时间
        uint64_t proc_time = 0;  // 进程总时间
    };
    CpuSnapshot last_cpu_;
    
    // 获取内存、线程数、上下文切换
    // 读取 /proc/self/status
    void get_proc_status(long& mem_kb, int& threads, long& ctxt_switches) {
        std::ifstream file("/proc/self/status");
        std::string line;
        while (std::getline(file, line)) {
            if (line.rfind("VmRSS:", 0) == 0) {
                mem_kb = parse_kb(line);
            } else if (line.rfind("Threads:", 0) == 0) {
                threads = std::stoi(line.substr(8));
            } else if (line.rfind("voluntary_ctxt_switches:", 0) == 0) {
                ctxt_switches = std::stol(line.substr(24));
            }
        }
    }

    // 辅助：解析 "VmRSS: 123 kB"
    long parse_kb(const std::string& line) {
        std::string val = line.substr(line.find_first_of("0123456789"));
        return std::stol(val);
    }

    // 获取 CPU 利用率 (核心逻辑)
    // 读取 /proc/stat (系统) 和 /proc/self/stat (进程)
    double get_cpu_usage() {
        // 1. 获取系统总时间
        std::ifstream stat_file("/proc/stat");
        std::string line;
        std::getline(stat_file, line); // 第一行是 "cpu  ..."
        std::istringstream iss(line.substr(5));
        uint64_t user, nice, system, idle, iowait, irq, softirq;
        iss >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
        uint64_t total_sys = user + nice + system + idle + iowait + irq + softirq;

        // 2. 获取进程时间 (utime + stime)
        std::ifstream self_stat("/proc/self/stat");
        std::string junk;
        uint64_t utime, stime;
        // 前13个字段跳过
        for(int i=0; i<13; ++i) self_stat >> junk;
        self_stat >> utime >> stime;
        uint64_t total_proc = utime + stime;

        // 3. 计算 Delta
        double percent = 0.0;
        if (last_cpu_.total_time != 0) {
            uint64_t diff_sys = total_sys - last_cpu_.total_time;
            uint64_t diff_proc = total_proc - last_cpu_.proc_time;
            
            // 核心数 (如果你是多核，可能超过 100%，要除以核心数)
            long num_processors = sysconf(_SC_NPROCESSORS_ONLN);
            
            if (diff_sys > 0) {
                percent = (double)diff_proc / diff_sys * num_processors * 100.0;
            }
        }

        // 更新快照
        last_cpu_ = {total_sys, total_proc};
        return percent;
    }

public:
    void status(const HttpContext& ctx, ResponseSender& sender) {
        // 1. 基础数据
        auto now = std::chrono::steady_clock::now();
        auto uptime_sec = std::chrono::duration_cast<std::chrono::seconds>(now - g_start_time).count();
        uint64_t req_count = g_total_requests.load();

        // 2. 流量数据
        uint64_t sent_bytes = g_bytes_sent.load();
        uint64_t recv_bytes = g_bytes_received.load();

        // 3. 连接数据
        int32_t connections = g_active_connections.load();

        // 4. 系统级数据 (/proc)
        long mem_kb = 0;
        int threads = 0;
        long ctxt_switches = 0;
        get_proc_status(mem_kb, threads, ctxt_switches);

        // 获取延迟数据
        uint64_t total_us = g_total_latency_us.load();
        // 【关键】读取并重置最大值为 0，给下一个周期用
        int max_us = g_max_latency_us.exchange(0); 
        
        double cpu_usage = get_cpu_usage();

        // 5. 构造 JSON
        nlohmann::json data;
        
        // --- 资源 ---
        data["memory_mb"] = mem_kb / 1024.0;
        data["cpu_percent"] = cpu_usage; // CPU利用率
        data["threads"] = threads;       // 线程数
        
        // --- 流量 & 负载 ---
        data["total_requests"] = req_count;
        data["connections"] = connections; // 并发连接数
        data["bytes_sent_mb"] = sent_bytes / 1024.0 / 1024.0;
        data["bytes_recv_mb"] = recv_bytes / 1024.0 / 1024.0;
        
        // --- 内核指标 (高级) ---
        data["ctxt_switches"] = ctxt_switches; // 上下文切换次数 (衡量锁竞争/系统调用)
        data["uptime_seconds"] = uptime_sec;

        data["total_latency_us"] = total_us; // 传给前端算平均
        data["max_latency_ms"] = max_us / 1000.0; // 直接转成毫秒

        sender(ctx.success(data));
    }
};

}