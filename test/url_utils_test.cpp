// 引入必要头文件
#include <gtest/gtest.h>
#include <string>
#include <map>
#include <stdexcept>
#include "common/logger.h"
// 引入被测模块头文件（根据实际路径调整）
#include "server/UrlUtils.h"  // 假设 UrlUtils 定义在该文件中
#include "const.h"
// 使用被测命名空间

using namespace cdfs;

// ====================== 测试 splitTarget 方法 ======================
TEST(UrlUtilsTest, SplitTarget_NormalCases) {
    // 场景1：正常带path和query的target
    std::string target1 = "/path?a=1&b=2";
    std::string path1, query1;
    UrlUtils::splitTarget(target1, path1, query1);
    EXPECT_EQ(path1, "/path");
    EXPECT_EQ(query1, "a=1&b=2");

    // 场景2：只有path，无query
    std::string target2 = "/api/user/detail";
    std::string path2, query2;
    UrlUtils::splitTarget(target2, path2, query2);
    EXPECT_EQ(path2, "/api/user/detail");
    EXPECT_EQ(query2, "");

    // 场景3：只有query，无path
    std::string target3 = "?name=test&age=18";
    std::string path3, query3;
    UrlUtils::splitTarget(target3, path3, query3);
    EXPECT_EQ(path3, "");
    EXPECT_EQ(query3, "name=test&age=18");

    // 场景4：空target
    std::string target4 = "";
    std::string path4, query4;
    UrlUtils::splitTarget(target4, path4, query4);
    EXPECT_EQ(path4, "");
    EXPECT_EQ(query4, "");

    // 场景5：多个?的情况（以第一个?为分隔）
    std::string target5 = "/file?version=1?type=txt";
    std::string path5, query5;
    UrlUtils::splitTarget(target5, path5, query5);
    EXPECT_EQ(path5, "/file");
    EXPECT_EQ(query5, "version=1?type=txt");
}

// ====================== 测试 decode 方法 ======================
TEST(UrlUtilsTest, Decode_NormalCases) {
    // 场景1：正常URL编码（%20=空格，%2F=/，%41=A，%E5%BC%A0%E4%B8%89=张三）
    std::string encoded1 = "%2Fpath%20test%41%E5%BC%A0%E4%B8%89";
    std::string decoded1 = UrlUtils::decode(encoded1);
    EXPECT_EQ(decoded1, "/path testA张三");

    // 场景2：空字符串解码
    std::string encoded2 = "";
    std::string decoded2 = UrlUtils::decode(encoded2);
    EXPECT_EQ(decoded2, "");

    // 场景3：无需解码的普通字符串
    std::string encoded3 = "normal-string_123";
    std::string decoded3 = UrlUtils::decode(encoded3);
    EXPECT_EQ(decoded3, "normal-string_123");
}

TEST(UrlUtilsTest, Decode_ExceptionCases) {
    // 场景1：无效的%编码（仅%无后续字符）
    std::string invalid1 = "/path%2";
    EXPECT_THROW(UrlUtils::decode(invalid1), UrlParseException);

    // 场景2：%后接非十六进制字符
    std::string invalid2 = "/path%g3";
    EXPECT_THROW(UrlUtils::decode(invalid2), UrlParseException);

    // 场景3：%后接不完整的十六进制（如%2g）
    std::string invalid3 = "%2gtest";
    EXPECT_THROW(UrlUtils::decode(invalid3), UrlParseException);
}

// ====================== 测试 parseQuery 方法 ======================
TEST(UrlUtilsTest, ParseQuery_NormalCases) {
    // 场景1：正常多键值对
    std::string query1 = "a=1&b=2&c=3";
    auto map1 = UrlUtils::parseQuery(query1);
    EXPECT_EQ(map1.size(), 3);
    EXPECT_EQ(map1["a"], "1");
    EXPECT_EQ(map1["b"], "2");
    EXPECT_EQ(map1["c"], "3");

    // 场景2：空query
    std::string query2 = "";
    auto map2 = UrlUtils::parseQuery(query2);
    EXPECT_EQ(map2.empty(), true);

    // 场景3：键有值为空
    std::string query3 = "a=&b=2&c=";
    auto map3 = UrlUtils::parseQuery(query3);
    EXPECT_EQ(map3["a"], "");
    EXPECT_EQ(map3["b"], "2");
    EXPECT_EQ(map3["c"], "");

    // 场景4：空键有值
    std::string query4 = "=123&b=456";
    auto map4 = UrlUtils::parseQuery(query4);
    EXPECT_EQ(map4[""], "123");
    EXPECT_EQ(map4["b"], "456");

    // 场景5：重复键（后值覆盖前值）
    std::string query5 = "a=1&a=999&b=2";
    auto map5 = UrlUtils::parseQuery(query5);
    EXPECT_EQ(map5["a"], "999");  // 后值覆盖
    EXPECT_EQ(map5["b"], "2");

}



// ====================== 端到端URL解析测试（核心） ======================
TEST(UrlUtilsTest, FullUrlParse_EndToEnd) {
    // 场景1：完整URL（含协议/主机/路径/参数/锚点）
    std::string fullUrl = "http://localhost:8080/api/user?name=张三%20san&age=18#info";
    // 步骤1：剥离协议/主机/端口（实际业务中可通过其他方法解析，此处仅测试path/query部分）
    std::string target = fullUrl.substr(fullUrl.find("/api")); // 截取 "/api/user?name=张三%20san&age=18#info"
    
    std::string path, query;
    UrlUtils::splitTarget(target, path, query);
    EXPECT_EQ(path, "/api/user");
    EXPECT_EQ(query, "name=张三%20san&age=18");

    // 步骤2：解析并解码查询参数
    auto params = UrlUtils::parseQuery(query);
    EXPECT_EQ(params["name"], "张三 san"); // %20 解码为空格
    EXPECT_EQ(params["age"], "18");

    // 场景2：无锚点、含特殊符号的URL
    std::string target2 = "/file/download?filename=report%202025%26%E5%B9%B4.xlsx";
    UrlUtils::splitTarget(target2, path, query);
    EXPECT_EQ(path, "/file/download");
    EXPECT_EQ(query, "filename=report%202025%26%E5%B9%B4.xlsx");
    
    auto params2 = UrlUtils::parseQuery(query);
    EXPECT_EQ(params2["filename"], "report 2025&年.xlsx"); // %26→&，%E5%B9%B4→年
}

// 测试主函数
int main(int argc, char **argv) {
    LOG_INIT();
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}