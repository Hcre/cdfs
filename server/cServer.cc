#include "cServer.h"
#include "common/logger.h"
#include "HttpSession.h"


namespace cdfs {

cServer::cServer(boost::asio::io_context &ioc, unsigned short port):
    _ioc(ioc),
    _acceptor(_ioc, tcp::endpoint(tcp::v4(), port)),
    _socket(_ioc)
{
    LOG_INFO << "server start listen at port " << port;
}

//接受连接
void cServer::start_accept()
{
    auto self = shared_from_this();
    _acceptor.async_accept(_socket,[self](boost::system::error_code ec) {
        try {
            if (ec) {
                LOG_ERROR << "accept error: " << ec.message();
                self->start_accept();
            }
            std::make_shared<HttpSession>(std::move(self->_socket))->run();
            self->start_accept();
        } catch (std::exception& e) {   
            std::cout << "exception is" << e.what() << std::endl;
        }
    });

}

}
