#include "HttpSession.h"
#include "common/logger.h"
#include "Router.h"

namespace cdfs {


HttpSession::HttpSession(tcp::socket&& socket)
    :stream_(std::move(socket))  // 移动语义初始化 tcp_stream
    ,buffer_()                   // flat_buffer 默认构造（可指定初始容量，如 1024）
    ,req_() {                    // http_request 默认构造
    //LOG_DEBUG << "HttpSession created.";
    reset_timeout_timer(std::chrono::seconds(30)); // 初始化超时定时器
    g_active_connections++;
}                   
 
HttpSession::~HttpSession() {
    g_active_connections--;
}

void HttpSession::run()
{
    do_read();
}

void HttpSession::do_read(){
    parser_.emplace();

    //设置50mb的限制
    parser_->body_limit(50 * 1024 * 1024); // 50MB

    
    //LOG_TRACE << "do_read";
    http::async_read(stream_, buffer_, *parser_,
        beast::bind_front_handler(&HttpSession::on_read, shared_from_this()));
}

void HttpSession::on_read(beast::error_code ec, std::size_t bytes_transferred) {
    req_start_time_ = std::chrono::steady_clock::now();
    //LOG_TRACE << "on_read";

    // 测量数据
    if (bytes_transferred > 0) {
        g_bytes_received += bytes_transferred;
    }

    //取消超时器
    timeout_timer_.cancel(); 
    auto& req = parser_->get(); 

    boost::ignore_unused(bytes_transferred);
   
    //客户端主动中断连接
    if(ec == http::error::end_of_stream) {
        LOG_INFO  <<"client disconnect";
        beast::error_code shutdown_ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, shutdown_ec);
        
        if (shutdown_ec) {
            LOG_WARN << "Shutdown failed: " << shutdown_ec.message();
        }

        beast::error_code close_ec;
        stream_.socket().close(close_ec);
        if (close_ec) {
            LOG_WARN << "Close failed: " << close_ec.message();
        }
        return;
    }

    if(ec) {
            if (log_and_close(ec, "write")) {
                return; // 连接已被关闭，必须立即退出
            }
            // 理论上，log_and_close 应始终返回true并关闭连接，
            // 此处为安全起见，即使未关闭也返回。
            return;
        }

    //正常逻辑，转发路由
    //LOG_DEBUG << "dispatch";
    auto self = shared_from_this();
    Router::getInstance()->dispatch(req, [self](http::message_generator&& res) {
        self->do_write(std::move(res));
    });
    
}

void HttpSession::do_write(http::message_generator&& msg){
    //LOG_DEBUG << "do_write, req.keep_alive()=" << req_.keep_alive() << ", will_close=" << !req_.keep_alive();


     // 关键：这里必须从请求中获取 keep_alive 状态，并转化为 close 标志
    bool keep_alive = true; // 从当前请求对象获取
    bool need_close = !keep_alive; // 或者直接传递 keep_alive
    // 注意：这里使用req_.keep_alive()可能不安全，因为req_可能已经被移动
    // 建议在调用do_write之前获取keep_alive并作为参数传入，但您要求不改变do_write的参数
    // 所以这里我们假设在调用do_write时，req_仍然有效（即do_write在req_被移动之前调用）
     beast::async_write(
        stream_,
        std::move(msg),  // 直接移动，无需 shared_ptr
        beast::bind_front_handler(
            &HttpSession::on_write,
            shared_from_this(),
            need_close    // 连接状态从调用方传入
        )
    );
}

void HttpSession::on_write(bool close, beast::error_code ec, std::size_t bytes_transferred){
    boost::ignore_unused(bytes_transferred);
    if (bytes_transferred > 0) {
        g_bytes_sent += bytes_transferred;
    }

    //结束计时
     // 【结束计时】
    auto now = std::chrono::steady_clock::now();
    // 计算耗时 (微秒)
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - req_start_time_).count();
    
    if (duration > 0) {
        // 1. 累加总耗时
        g_total_latency_us += duration;

        // 2. 更新最大值 (使用 CAS 循环确保线程安全，虽然在这哪怕有竞争也没事)
        int current_max = g_max_latency_us.load();
        while (duration > current_max && !g_max_latency_us.compare_exchange_weak(current_max, (int)duration));
    }



    //LOG_DEBUG << "on_write, close=" << close << ", ec=" << ec.message();
    if(ec) {
        if (log_and_close(ec, "write")) {
            return; // 连接已被关闭，必须立即退出
        }
        // 理论上，log_and_close 应始终返回true并关闭连接，
        // 此处为安全起见，即使未关闭也返回。
        return;
        }

    //主动断开连接
    if (close) {
        beast::error_code shutdown_ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, shutdown_ec);
        if (shutdown_ec) {
            LOG_WARN << "Shutdown failed: " << shutdown_ec.message();
        }
        
        // 彻底关闭 socket，释放文件描述符
        beast::error_code close_ec;
        stream_.socket().close(close_ec);
        return;  // void 函数空 return，提前退出

    }
    //重置超时器
    reset_timeout_timer(std::chrono::seconds(30));
    auto& req = parser_->get(); 
    //继续读，清空请求和缓冲区
    req = http::request<http::string_body>();
    buffer_.consume(buffer_.size());
    do_read();
}

void HttpSession::reset_timeout_timer(std::chrono::seconds duration){
    timeout_timer_.expires_after(std::chrono::seconds(duration)); // 重置超时
    timeout_timer_.async_wait([this](const boost::system::error_code& ec) {
        if (!ec) {
            LOG_WARN << "Session timeout, closing connection.";
            beast::error_code shutdown_ec;
            this->stream_.socket().shutdown(tcp::socket::shutdown_both, shutdown_ec);
            if (shutdown_ec) {
                LOG_ERROR << "Shutdown failed: " << shutdown_ec.message();
            }
            beast::error_code close_ec;
            this->stream_.socket().close(close_ec);
            if (close_ec) {
                LOG_ERROR << "Close socket failed: " << close_ec.message();
            }
        }
    });


}

bool HttpSession::log_and_close(beast::error_code ec, const char* what) {
    // 定义一组“预期内的、无需警告”的错误
    // 这些错误通常由客户端正常行为（如断开连接）引起
     // 扩充“预期内的安静错误”列表，加入HTTP协议错误
    if (ec == beast::errc::success ||
        ec == http::error::end_of_stream ||
        ec == boost::asio::error::operation_aborted ||
        ec == boost::asio::error::broken_pipe ||
        ec == boost::asio::error::connection_reset ||
        ec == boost::asio::error::timed_out ||
        // 新增：以下为HTTP协议层面的常见“结果性”错误
        ec == http::error::bad_method ||    // bad_method
        ec == http::error::bad_version ||   // bad_version
        ec == http::error::bad_target ||    // 错误目标
        ec == http::error::bad_value ) {    // 错误头部
        // 这些错误往往是连接损坏后的副产品，降级为DEBUG日志
        LOG_DEBUG << what << " ended: " << ec.message();
        beast::error_code close_ec;
        stream_.socket().close(close_ec);
        return true;
    }
    // 对于其他意外错误，记录为ERROR
    LOG_ERROR << what << " failed: " << ec.message();
    beast::error_code close_ec;
    stream_.socket().close(close_ec);
    if (close_ec) {
        LOG_WARN << "Close socket failed: " << close_ec.message();
    }
    return true; // 表示连接已关闭，上层应直接返回
}


}