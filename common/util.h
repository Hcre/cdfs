#include <string>
#include <cassert>
#include "const.h"
namespace cdfs {

unsigned char ToHex(unsigned char x)
{
	return  x > 9 ? x + 55 : x + 48;
}

unsigned char FromHex(unsigned char x)
{
	unsigned char y;
	if (x >= 'A' && x <= 'Z') y = x - 'A' + 10;
	else if (x >= 'a' && x <= 'z') y = x - 'a' + 10;
	else if (x >= '0' && x <= '9') y = x - '0';
	else assert(0);
	return y;
}


std::string UrlDecode(const std::string& str)
{
	std::string strTemp = "";
	size_t length = str.length();
	for (size_t i = 0; i < length; i++)
	{
		//还原+为空
		if (str[i] == '+') strTemp += ' ';
		//遇到%将后面的两个字符从16进制转为char再拼接
		else if (str[i] == '%')
		{
			assert(i + 2 < length);
			unsigned char high = FromHex((unsigned char)str[++i]);
			unsigned char low = FromHex((unsigned char)str[++i]);
			strTemp += high * 16 + low;
		}
		else strTemp += str[i];
	}
	return strTemp;
}


// http::verb 转字符串（用于日志/调试）
std::string VerbToString(http::verb method) {
    switch (method) {
        case http::verb::get: return "GET";
        case http::verb::post: return "POST";
        case http::verb::put: return "PUT";
        case http::verb::delete_: return "DELETE";
        case http::verb::head: return "HEAD";
        default: return "UNKNOWN";
    }
}




// ===================== 工具函数：字节序转换（跨平台） =====================
// 32位无符号整数 主机序 → 网络序（大端）
inline uint32_t host_to_net32(uint32_t val) {
    return htonl(val);
}

// 32位无符号整数 网络序 → 主机序
inline uint32_t net_to_host32(uint32_t val) {
    return ntohl(val);
}

// 64位无符号整数 主机序 → 网络序（大端）
inline uint64_t host_to_net64(uint64_t val) {
    // 手动实现64位字节序转换（htonll/ntohll非标准）
    uint64_t res = 0;
    for (int i = 0; i < 8; ++i) {
        res = (res << 8) | ((val >> (i * 8)) & 0xFF);
    }
    return res;
}

// 64位无符号整数 网络序 → 主机序
inline uint64_t net_to_host64(uint64_t val) {
    uint64_t res = 0;
    for (int i = 0; i < 8; ++i) {
        res = (res << 8) | ((val >> ((7 - i) * 8)) & 0xFF);
    }
    return res;
}

// time_t 本质是int64_t/long，统一按64位处理
inline uint64_t time_to_net(time_t val) {
    return host_to_net64(static_cast<uint64_t>(val));
}

inline time_t net_to_time(uint64_t val) {
    return static_cast<time_t>(net_to_host64(val));
}

}