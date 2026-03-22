#include <gtest/gtest.h>
#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include "MetadataStore.h"
#include "common/logger.h"

// 命名空间别名，简化代码
namespace fs = std::filesystem;
using namespace cdfs;

// ====================== 测试夹具 ======================
class MetadataStoreTest : public ::testing::Test {
protected:
    // 每个测试用例执行前初始化
    void SetUp() override {
        // 1. 创建临时LevelDB目录（基于系统临时目录）
        std::string dir_name = "metadata_store_test_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed());
        temp_db_path_ = fs::temp_directory_path() / dir_name;
        fs::create_directories(temp_db_path_);
        // 2. 初始化MetadataStore（构造函数打开DB）
        try {
            store_ = std::make_unique<MetadataStore>(temp_db_path_.string());
        } catch (...) {
            FAIL() << "MetadataStore构造失败，路径：" << temp_db_path_.string();
        }

        // 3. 构造测试用的元数据
        test_meta_.md5 = "1234567890abcdef1234567890abcdef"; // 32位MD5
        test_meta_.path = "/group1/test.txt";
        test_meta_.scene = "default";
        test_meta_.mtime = 1680000000; // 时间戳
        test_meta_.size = 1024;        // 1KB
        test_meta_.status = FileStatus::NORMAL; // 正常状态
    }

    // 每个测试用例执行后清理
    void TearDown() override {
        // 销毁store（析构函数关闭DB）
        store_.reset();
        // 删除临时目录（清理LevelDB文件）
        if (fs::exists(temp_db_path_)) {
            fs::remove_all(temp_db_path_);
        }
    }

    // 测试用成员变量
    fs::path temp_db_path_;                  // 临时DB路径
    std::unique_ptr<MetadataStore> store_;   // MetadataStore实例
    MetaFile test_meta_;                     // 测试用元数据
};

// ====================== 测试用例 ======================

// 1. 构造/析构测试：能正常创建和销毁，无崩溃
TEST_F(MetadataStoreTest, ConstructorDestructor) {
    // 只要SetUp不抛异常，说明构造成功
    ASSERT_TRUE(store_ != nullptr);
}

// 2. save_meta + get_meta测试：保存后能正确查询
TEST_F(MetadataStoreTest, SaveAndGetMeta) {
    // 2.1 保存元数据
    bool save_ok = store_->save_meta(test_meta_);
    ASSERT_TRUE(save_ok) << "元数据保存失败";

    // 2.2 通过MD5查询
    std::optional<MetaFile> got_meta = store_->get_meta(test_meta_.md5);
    ASSERT_TRUE(got_meta.has_value()) << "通过MD5查询不到保存的元数据";
    ASSERT_EQ(got_meta->md5, test_meta_.md5);
    ASSERT_EQ(got_meta->path, test_meta_.path);
    ASSERT_EQ(got_meta->size, test_meta_.size);
    ASSERT_EQ(got_meta->status, test_meta_.status);

    // 2.3 通过路径查询（内部会转MD5）
    std::optional<MetaFile> got_meta_by_path = store_->get_meta(test_meta_.path);
    ASSERT_TRUE(got_meta_by_path.has_value()) << "通过路径查询不到保存的元数据";
    ASSERT_EQ(got_meta_by_path->md5, test_meta_.md5);

    // 2.4 查询不存在的file_id，返回nullopt
    std::optional<MetaFile> no_meta = store_->get_meta("invalid_file_id");
    ASSERT_FALSE(no_meta.has_value()) << "查询不存在的file_id应返回nullopt";
}

// 3. exists测试：存在返回true，不存在返回false
TEST_F(MetadataStoreTest, Exists) {
    // 3.1 未保存时，不存在
    ASSERT_FALSE(store_->exists(test_meta_.md5)) << "未保存的元数据应返回不存在";

    // 3.2 保存后，存在
    store_->save_meta(test_meta_);
    ASSERT_TRUE(store_->exists(test_meta_.md5)) << "保存的元数据应返回存在";
    ASSERT_TRUE(store_->exists(test_meta_.path)) << "通过路径查询存在性应返回true";

    // 3.3 不存在的file_id返回false
    ASSERT_FALSE(store_->exists("invalid_md5"));
}

// 4. update_status测试：更新状态后查询正确
TEST_F(MetadataStoreTest, UpdateStatus) {
    // 4.1 先保存元数据
    store_->save_meta(test_meta_);
    std::optional<MetaFile> before_update = store_->get_meta(test_meta_.md5);
    ASSERT_EQ(before_update->status, FileStatus::NORMAL);

    // 4.2 更新状态为DELETED
    bool update_ok = store_->update_status(test_meta_.md5, FileStatus::DELETED);
    ASSERT_TRUE(update_ok) << "更新状态失败";

    // 4.3 验证状态已更新
    std::optional<MetaFile> after_update = store_->get_meta(test_meta_.md5);
    ASSERT_EQ(after_update->status, FileStatus::DELETED);
}

// 5. delete_meta测试：删除后无法查询
TEST_F(MetadataStoreTest, DeleteMeta) {
    // 5.1 先保存元数据
    store_->save_meta(test_meta_);
    ASSERT_TRUE(store_->exists(test_meta_.md5));

    // 5.2 删除元数据
    bool delete_ok = store_->delete_meta(test_meta_.md5);
    ASSERT_TRUE(delete_ok) << "删除元数据失败";

    // 5.3 验证已删除
    ASSERT_FALSE(store_->exists(test_meta_.md5));
    std::optional<MetaFile> deleted_meta = store_->get_meta(test_meta_.md5);
    ASSERT_FALSE(deleted_meta.has_value());
}

// 6. get_stat + repair_stat测试：统计值正确
TEST_F(MetadataStoreTest, StatAndRepairStat) {
    // 6.1 初始状态：统计值为0
    SystemStat init_stat = store_->get_stat();
    ASSERT_EQ(init_stat.file_count, 0);
    ASSERT_EQ(init_stat.total_size, 0);

    // 6.2 保存元数据后，统计值更新
    store_->save_meta(test_meta_);
    SystemStat after_save_stat = store_->get_stat();
    ASSERT_EQ(after_save_stat.file_count, 1);
    ASSERT_EQ(after_save_stat.total_size, 1024);

    // 6.3 手动修改统计（模拟异常），然后修复
    // 注意：cached_stat_是私有成员，这里通过repair_stat重新计算
    // 先删除元数据（让统计值虚高）
    store_->delete_meta(test_meta_.md5);
    
    store_->repair_stat(); // 修复统计
    SystemStat repaired_stat = store_->get_stat();
    ASSERT_EQ(repaired_stat.file_count, 0);
    ASSERT_EQ(repaired_stat.total_size, 0);
}

TEST_F(MetadataStoreTest, ListDir) {
    // 7.1 批量保存5个【有效】元数据（32位MD5 + default场景 + NORMAL状态）
    for (int i = 0; i < 5; i++) {
        MetaFile meta = test_meta_;
        // 构造32位MD5（核心修正：前31位固定字符 + 最后1位补i，总长度32）
        meta.md5 = "1234567890abcdef1234567890abcde" + std::to_string(i);
        // 强制指定scene为"default"（匹配查询的dir）
        meta.scene = "default";
        // 强制指定status为NORMAL（避免被过滤）
        meta.status = FileStatus::NORMAL;
        // 时间戳递增（用于排序验证）
        meta.mtime = 1680000000 + i;
        meta.size = 1024 + i;

        // 断言保存成功（验证数据真的写入LevelDB）
        bool save_ok = store_->save_meta(meta);
        ASSERT_TRUE(save_ok) << "保存测试元数据" << i << "失败（MD5=" << meta.md5 << "）";
        // 断言数据存在（验证LevelDB写入成功）
        ASSERT_TRUE(store_->exists(meta.md5)) << "元数据" << i << "保存后不存在（MD5=" << meta.md5 << "）";
    }

    // 7.2 测试升序分页：offset=1, limit=2（取第2、3条，索引从0开始）
    std::vector<MetaFile> list_asc = store_->list_dir("default", 1, 2, 1); // 1=升序
    ASSERT_EQ(list_asc.size(), 2) << "升序分页结果数量错误（实际=" << list_asc.size() << "，预期=2）";
    // 验证排序：升序时mtime递增
    ASSERT_LE(list_asc[0].mtime, list_asc[1].mtime) << "升序排序错误：mtime1=" << list_asc[0].mtime << " > mtime2=" << list_asc[1].mtime;

    // 7.3 测试降序分页：offset=0, limit=3（取前3条，按时间降序）
    std::vector<MetaFile> list_desc = store_->list_dir("default", 0, 3, 2); // 2=降序
    ASSERT_EQ(list_desc.size(), 3) << "降序分页结果数量错误（实际=" << list_desc.size() << "，预期=3）";
    // 验证排序：降序时mtime递减
    ASSERT_GE(list_desc[0].mtime, list_desc[1].mtime) << "降序排序错误：mtime1=" << list_desc[0].mtime << " < mtime2=" << list_desc[1].mtime;

    // 7.4 测试异常参数：offset=-1, limit=0 → 返回空
    std::vector<MetaFile> list_invalid = store_->list_dir("default", -1, 0, 1);
    ASSERT_TRUE(list_invalid.empty()) << "异常参数应返回空列表（实际=" << list_invalid.size() << "）";
}

// ====================== 主函数 ======================
int main(int argc, char **argv) {
    LOG_INIT();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}