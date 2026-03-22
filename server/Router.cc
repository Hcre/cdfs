#include "Router.h"
#include "UrlUtils.h"
#include "common/util.h"
#include "common/logger.h"
namespace cdfs {

void Router::add_route(http::verb method, const std::string &path, HandlerFunction handler){
    std::lock_guard<std::mutex> lock(router_mtx_);
    routers_[{method, path}] = handler;
    
}

// 2. 注册前缀路由 (例如 /group1/view/)
void Router::add_prefix(http::verb method, const std::string& prefix, HandlerFunction handler) {
    // 存入 Vector
    LOG_DEBUG << "add_prefix: " <<  prefix;
    prefix_routes_.push_back({method, prefix, handler});
    // [关键优化] 每次添加后，按前缀长度降序排序
    // 保证 "/group1/view/" 优先于 "/group1/" 被匹配
    std::sort(prefix_routes_.begin(), prefix_routes_.end(), 
        [](const PrefixRouteEntry& a, const PrefixRouteEntry& b) {
            return a.prefix.length() > b.prefix.length();
        });

    LOG_DEBUG << "prefix_routes_ size: " << prefix_routes_.size();
}

void Router::dispatch(const StringRequest &req, std::function<void(http::message_generator)> sender){
    g_total_requests++;
    //路由解析
    //提取出url中的path
    std::string raw_path, raw_query, base_path, path_suffix;
    UrlUtils::splitTarget(std::string(req.target()), raw_path, raw_query);
    std::string method = VerbToString(req.method());// 请求方法（字符串形式）

    //如果有params，提取出params
    std::map<std::string, std::string> query_params = UrlUtils::parseQuery(raw_query);

    // std::pair<std::string, std::string> path_and_suffix = UrlUtils::extractPathAndSuffix(raw_path);
     auto it = routers_.find({req.method(), raw_path});
    if (it != routers_.end()) {
    // 构造 Context
        base_path = it->first.second;
        HttpContext ctx{req, method, raw_path, query_params, ""}; 
        it->second(ctx, std::move(sender));
        return;
    }
    //LOG_DEBUG << "dispatch: " << method << " " << raw_path;
    for (const auto& route: prefix_routes_) {
        if (req.method() != route.method) {
            continue;
        }
       // 2. 检查前缀匹配（raw_path以route.prefix开头）
        if (raw_path.find(route.prefix) != 0) {
            continue;
        }

        const size_t prefix_len = route.prefix.size();
        if (prefix_len > raw_path.size()) {
            LOG_ERROR << "prefix length(" << prefix_len << ") > raw_path length(" << raw_path.size() 
                    << ") for prefix: " << route.prefix;
            continue;
        }
        // 4. 提取后缀路径
        std::string suffix = raw_path.substr(prefix_len);
        HttpContext ctx{req, method, raw_path, query_params, suffix};

        // 防御3：检查handler是否为空（避免调用空函数触发段错误）
        if (!route.handler) {
            LOG_ERROR << "empty handler for prefix: " << route.prefix;
            sender(ctx.not_found("Internal Server Error"));
            return;
        }
          // 调用正确的回调函数
        route.handler(ctx, std::move(sender));
        //LOG_DEBUG << "dispatch: matched prefix " << route.prefix << " for " << raw_path;
        return;

    }

    //LOG_DEBUG << "dispatch succeed";

    not_found(req, std::move(sender));

}

void Router::not_found(const StringRequest &req,std::function<void(http::message_generator)> sender){
    http::response<http::string_body> res{http::status::not_found, req.version()};
    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = "404 Not Found - Powered by Boost.Beast";
    res.prepare_payload();
    sender(std::move(res));
}

}