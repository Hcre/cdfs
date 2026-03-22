#ifndef __CSERVER_H__
#define __CSERVER_H__
#include "const.h"

namespace cdfs {

class cServer: public std::enable_shared_from_this<cServer> {
public:
    cServer(boost::asio::io_context& ioc, unsigned short port);

    //接收连接
    void start_accept();

private:
    boost::asio::io_context& _ioc;
    tcp::acceptor _acceptor;
    tcp::socket _socket; //一个可复用的连接

};
}

#endif