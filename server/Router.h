#ifndef __ROUTER_H__
#define __ROUTER_H__

#include "const.h"
#include "common/Singleton.h"
#include "HttpContext.h"
namespace cdfs {



class Router: public singleton<Router> {
public:
    friend class singleton<Router>;
        // 注册路由：Method + Path -> Handler
    void add_route(http::verb method, const std::string& path, HandlerFunction handler);
    void add_prefix(http::verb method, const std::string& prefix, HandlerFunction handler);

    void dispatch(const StringRequest& req, std::function<void(http::message_generator)> sender);

    //404
    void not_found(const StringRequest& req, std::function<void(http::message_generator)> sender);
    
    //
private:
    std::mutex router_mtx_;
    std::map<std::pair<http::verb, std::string>, HandlerFunction> routers_;
    
    struct PrefixRouteEntry {
        http::verb method;
        std::string prefix;
        HandlerFunction handler;
    };

    std::vector<PrefixRouteEntry> prefix_routes_;

};



}



#endif