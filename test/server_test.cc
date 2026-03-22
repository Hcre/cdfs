#include "server/Router.h"
#include "server/cServer.h"
#include "const.h"
#include "common/logger.h"
#include "controller/FileCtl.h"
#include "config/StoreConfig.h"
#include "server/HttpContext.h"
#include "config/metaConfig.h"
#include "controller/SystemCtl.h"
#include <thread>
#include <vector>

using namespace cdfs;

struct AppConfig {
    StoreConfig store;
    metaConfig meta;
    std::string db_path;
};


int main() {
    cdfs::g_start_time = std::chrono::steady_clock::now();
    LOG_INIT();
    
    // 使用 shared_ptr 创建控制器对象
    auto system_ctl = std::make_shared<SystemCtl>();
    
    AppConfig app_config;
    app_config.db_path = "test.db";
    
    auto file_ctl = std::make_shared<FileCtl>(
        std::make_shared<FileStore>(app_config.store),
        std::make_shared<MetadataStore>(app_config.db_path)
    );
    
    // 注册所有路由 - 按值捕获 shared_ptr
    auto& router = *cdfs::Router::getInstance();
    
    // POST /check
    router.add_route(
        http::verb::post,
        "/check",
        [file_ctl](const HttpContext& ctx, ResponseSender sender) {
            file_ctl->handleCheck(ctx, sender);
        }
    );
    
    // POST /upload
    router.add_route(
        http::verb::post,
        "/upload",
        [file_ctl](const HttpContext& ctx, ResponseSender sender) {
            file_ctl->handleUpload(ctx, sender);
        }
    );
    
    // GET /group1/*
    router.add_prefix(
        http::verb::get,
        "/group1",
        [file_ctl](const HttpContext& ctx, ResponseSender sender) {
            file_ctl->handleDownload(ctx, sender);
        }
    );
    
    // GET /check_chunk
    router.add_route(
        http::verb::get,
        "/check_chunk",
        [file_ctl](const HttpContext& ctx, ResponseSender sender) {
            file_ctl->handleCheckChunk(ctx, sender);
        }
    );
    
    // GET /upload_chunk
    router.add_route(
        http::verb::get,
        "/upload_chunk",
        [file_ctl](const HttpContext& ctx, ResponseSender sender) {
            file_ctl->handleChunkUpload(ctx, sender);
        }
    );
    
    // GET /status
    router.add_route(
        http::verb::get,
        "/status",
        [system_ctl](const HttpContext& ctx, ResponseSender sender) {
            system_ctl->status(ctx, sender);
        }
    );

    router.add_route(
        http::verb::post,
        "/merge",
        [file_ctl](const HttpContext& ctx, ResponseSender sender) {
            file_ctl->handleMerge(ctx, sender);
        }
    );
    
    // 启动多线程服务器
    try {
        const int num_threads = std::thread::hardware_concurrency(); // 使用CPU核心数
        LOG_INFO << "Starting server with " << num_threads << " worker threads";
        
        boost::asio::io_context ioc;
        
        auto server = std::make_shared<cdfs::cServer>(ioc, 8080);
        server->start_accept();
        
        // 创建工作线程池
        std::vector<std::thread> threads;
        threads.reserve(num_threads);
        
        // 启动工作线程
        for(int i = 0; i < num_threads; ++i) {
            threads.emplace_back([&ioc] {
                try {
                    ioc.run();
                } catch (const std::exception& e) {
                    LOG_ERROR << "IO context exception: " << e.what();
                }
            });
        }
        
        LOG_INFO << "Server started on port 8080";
        
        // 等待所有线程完成（通常不会返回）
        for(auto& thread : threads) {
            thread.join();
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Fatal exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}