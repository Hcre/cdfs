#ifndef __HTTP_SESSION_H__
#define __HTTP_SESSION_H__

#include "const.h"
#include <boost/optional.hpp> 
namespace cdfs {

class HttpSession:public std::enable_shared_from_this<HttpSession> {
private:
    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    http::request<http::string_body> req_;

    boost::optional<http::request_parser<http::string_body>> parser_;
    
    //todo: file_body实现零拷贝

    boost::asio::steady_timer timeout_timer_{stream_.socket().get_executor()}; // 超时定时器

    std::chrono::steady_clock::time_point req_start_time_; // 请求开始时间

public:
    HttpSession(tcp::socket&& socket);
    ~HttpSession();
public:
    void run();
    void do_read();
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void do_write(http::message_generator&& msg);
    void on_write(bool close, beast::error_code ec, std::size_t bytes_transferred);

    // //文件传输
    // void do_write(const FileResponse&& res)
    // void on_write(bool close, std::shared_ptr<FileResponse> res, beast::error_code ec, std::size_t bytes_transferred);

    void reset_timeout_timer(std::chrono::seconds duration);

    bool log_and_close(beast::error_code ec, const char* what);
};

}

#endif