// test_user_data_migration.cpp — 共用用户数据目录迁移到便携实例目录
//
// 在临时目录里搭一个假的 WebView2 用户数据目录，核对：哪些条目复制、哪些作为可重建
// 的缓存跳过；目标已存在时不碰它；中途失败不留下半个目标；锁文件被独占时判为占用。
#include "pch.h"
#include "webview/UserDataMigration.h"

#include <fstream>

namespace user_data_migration_test {

namespace fs = std::filesystem;
using webview_udf::CopyOutcome;

class UserDataMigrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        root_ = fs::temp_directory_path() / L"foo_ui_webview2_udf_test" /
                (std::string(info->test_case_name()) + "_" + info->name());
        std::error_code ec;
        fs::remove_all(root_, ec);
        fs::create_directories(root_);
        source_ = root_ / L"shared";
        target_ = root_ / L"profile" / L"foo_ui_webview2";
        fs::create_directories(target_.parent_path());
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    static void Write(const fs::path& file, const std::string& body = "x") {
        fs::create_directories(file.parent_path());
        std::ofstream(file, std::ios::binary) << body;
    }

    static std::string Read(const fs::path& file) {
        std::ifstream in(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), {});
    }

    // 一个运行过的 WebView2 用户数据目录的骨架：页面数据、浏览器状态与几类缓存
    void MakeSharedFolder() {
        Write(source_ / L"EBWebView" / L"Local State", "state");
        Write(source_ / L"EBWebView" / L"Default" / L"Local Storage" / L"leveldb" / L"000003.log", "ls");
        Write(source_ / L"EBWebView" / L"Default" / L"IndexedDB" /
              L"https_foo-ui-webview2.local_0.indexeddb.leveldb" / L"CURRENT", "idb");
        Write(source_ / L"EBWebView" / L"Default" / L"Network" / L"Cookies", "cookies");
        Write(source_ / L"EBWebView" / L"Default" / L"Cache" / L"Cache_Data" / L"data_0");
        Write(source_ / L"EBWebView" / L"Default" / L"Code Cache" / L"js" / L"index");
        Write(source_ / L"EBWebView" / L"Default" / L"GPUCache" / L"data_1");
        Write(source_ / L"EBWebView" / L"GrShaderCache" / L"data_0");
        Write(source_ / L"EBWebView" / L"Crashpad" / L"settings.dat");
    }

    fs::path root_;
    fs::path source_;
    fs::path target_;
};

TEST(UserDataMigrationRebuildable, CachesCrashDumpsAndTheLockAreSkipped) {
    EXPECT_TRUE(webview_udf::IsRebuildable(L"EBWebView\\lockfile"));
    EXPECT_TRUE(webview_udf::IsRebuildable(L"EBWebView\\Crashpad"));
    EXPECT_TRUE(webview_udf::IsRebuildable(L"EBWebView\\Default\\Cache"));
    EXPECT_TRUE(webview_udf::IsRebuildable(L"EBWebView\\Default\\Code Cache"));
    EXPECT_TRUE(webview_udf::IsRebuildable(L"EBWebView/Default/GPUCache"));
}

TEST(UserDataMigrationRebuildable, ComparisonIgnoresCase) {
    EXPECT_TRUE(webview_udf::IsRebuildable(L"ebwebview\\default\\cache"));
}

TEST(UserDataMigrationRebuildable, PageDataAndBrowserStateAreKept) {
    EXPECT_FALSE(webview_udf::IsRebuildable(L"EBWebView\\Local State"));
    EXPECT_FALSE(webview_udf::IsRebuildable(L"EBWebView\\Default\\Local Storage"));
    EXPECT_FALSE(webview_udf::IsRebuildable(L"EBWebView\\Default\\IndexedDB"));
    EXPECT_FALSE(webview_udf::IsRebuildable(L"EBWebView\\Default\\Network"));
    EXPECT_FALSE(webview_udf::IsRebuildable(L"EBWebView\\Default\\Service Worker"));
    // 只按完整条目匹配：名字相近的目录不受牵连
    EXPECT_FALSE(webview_udf::IsRebuildable(L"EBWebView\\Default\\Cache2"));
    EXPECT_FALSE(webview_udf::IsRebuildable(L"EBWebView\\Default\\Service Worker\\CacheStorage"));
}

TEST_F(UserDataMigrationTest, CopiesPageDataAndBrowserStateButNotCaches) {
    MakeSharedFolder();
    std::wstring error;
    EXPECT_EQ(webview_udf::CopyUserDataFolder(source_, target_, &error), CopyOutcome::Copied)
        << fs::path(error).string();

    EXPECT_EQ(Read(target_ / L"EBWebView" / L"Local State"), "state");
    EXPECT_EQ(Read(target_ / L"EBWebView" / L"Default" / L"Local Storage" / L"leveldb" / L"000003.log"), "ls");
    EXPECT_EQ(Read(target_ / L"EBWebView" / L"Default" / L"IndexedDB" /
                   L"https_foo-ui-webview2.local_0.indexeddb.leveldb" / L"CURRENT"), "idb");
    EXPECT_EQ(Read(target_ / L"EBWebView" / L"Default" / L"Network" / L"Cookies"), "cookies");

    EXPECT_FALSE(fs::exists(target_ / L"EBWebView" / L"Default" / L"Cache"));
    EXPECT_FALSE(fs::exists(target_ / L"EBWebView" / L"Default" / L"Code Cache"));
    EXPECT_FALSE(fs::exists(target_ / L"EBWebView" / L"Default" / L"GPUCache"));
    EXPECT_FALSE(fs::exists(target_ / L"EBWebView" / L"GrShaderCache"));
    EXPECT_FALSE(fs::exists(target_ / L"EBWebView" / L"Crashpad"));
}

TEST_F(UserDataMigrationTest, LeavesTheSourceInPlace) {
    MakeSharedFolder();
    ASSERT_EQ(webview_udf::CopyUserDataFolder(source_, target_), CopyOutcome::Copied);
    EXPECT_EQ(Read(source_ / L"EBWebView" / L"Local State"), "state");
    EXPECT_TRUE(fs::exists(source_ / L"EBWebView" / L"Default" / L"Cache" / L"Cache_Data" / L"data_0"));
}

TEST_F(UserDataMigrationTest, AnExistingTargetIsNeverTouched) {
    MakeSharedFolder();
    Write(target_ / L"EBWebView" / L"Local State", "own");
    EXPECT_EQ(webview_udf::CopyUserDataFolder(source_, target_), CopyOutcome::NotNeeded);
    EXPECT_EQ(Read(target_ / L"EBWebView" / L"Local State"), "own");
    EXPECT_FALSE(fs::exists(target_ / L"EBWebView" / L"Default"));
}

TEST_F(UserDataMigrationTest, NoSourceMeansNothingToDo) {
    EXPECT_EQ(webview_udf::CopyUserDataFolder(source_, target_), CopyOutcome::NotNeeded);
    EXPECT_FALSE(fs::exists(target_));
}

TEST_F(UserDataMigrationTest, AStagingFolderLeftByAnEarlierRunIsReplaced) {
    MakeSharedFolder();
    fs::path staging = target_;
    staging += L".migrating";
    Write(staging / L"stale.txt");
    ASSERT_EQ(webview_udf::CopyUserDataFolder(source_, target_), CopyOutcome::Copied);
    EXPECT_FALSE(fs::exists(staging));
    EXPECT_FALSE(fs::exists(target_ / L"stale.txt"));
}

TEST_F(UserDataMigrationTest, AFailedCopyLeavesNeitherTargetNorStaging) {
    MakeSharedFolder();
    const fs::path locked = source_ / L"EBWebView" / L"Default" / L"Network" / L"Cookies";
    // 独占打开，复制读不了它
    HANDLE handle = CreateFileW(locked.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(handle, INVALID_HANDLE_VALUE);
    std::wstring error;
    const CopyOutcome outcome = webview_udf::CopyUserDataFolder(source_, target_, &error);
    CloseHandle(handle);

    EXPECT_EQ(outcome, CopyOutcome::Failed);
    EXPECT_FALSE(error.empty());
    EXPECT_FALSE(fs::exists(target_));
    fs::path staging = target_;
    staging += L".migrating";
    EXPECT_FALSE(fs::exists(staging));
}

TEST_F(UserDataMigrationTest, AnExclusivelyOpenLockfileMeansInUse) {
    MakeSharedFolder();
    EXPECT_FALSE(webview_udf::IsInUse(source_));  // 没有锁文件

    const fs::path lockfile = source_ / L"EBWebView" / L"lockfile";
    HANDLE handle = CreateFileW(lockfile.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(handle, INVALID_HANDLE_VALUE);
    EXPECT_TRUE(webview_udf::IsInUse(source_));
    EXPECT_TRUE(webview_udf::WaitWhileInUse(source_, std::chrono::milliseconds(0)));
    CloseHandle(handle);

    // 锁文件留着但没人打开：浏览器被强杀时就是这样
    EXPECT_FALSE(webview_udf::IsInUse(source_));
    EXPECT_FALSE(webview_udf::WaitWhileInUse(source_, std::chrono::milliseconds(0)));
}

}  // namespace user_data_migration_test
