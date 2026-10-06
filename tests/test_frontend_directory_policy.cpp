// test_frontend_directory_policy.cpp - 前端目录候选的查找顺序与 index.html 判定
//
// 直接链接生产符号 frontend_directory_policy::*，在临时目录里搭出各级候选。
#include "pch.h"
#include "core/FrontendDirectoryPolicy.h"

#include <filesystem>
#include <fstream>

using frontend_directory_policy::Candidates;
using frontend_directory_policy::HasIndexHtml;
using frontend_directory_policy::Resolve;
using frontend_directory_policy::Source;

namespace fs = std::filesystem;

namespace {

class FrontendDirectoryPolicyTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::random_device rd;
        root_ = fs::temp_directory_path() / ("fb2k_frontend_dir_" + std::to_string(rd()));
        fs::create_directories(root_);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(root_, ec);
    }

    // 建一个只有目录、没有入口文件的候选
    std::wstring EmptyDir(const wchar_t* name) const {
        const fs::path dir = root_ / name;
        fs::create_directories(dir);
        return dir.wstring();
    }

    // 建一个带 index.html 的候选
    std::wstring TemplateDir(const wchar_t* name) const {
        const fs::path dir = root_ / name;
        fs::create_directories(dir);
        std::ofstream(dir / "index.html") << "<!DOCTYPE html>";
        return dir.wstring();
    }

    std::wstring MissingDir(const wchar_t* name) const { return (root_ / name).wstring(); }

    fs::path root_;
};

}  // namespace

TEST_F(FrontendDirectoryPolicyTest, HasIndexHtmlNeedsAFileNamedIndexHtml) {
    EXPECT_TRUE(HasIndexHtml(TemplateDir(L"ok")));
    EXPECT_FALSE(HasIndexHtml(EmptyDir(L"empty")));
    EXPECT_FALSE(HasIndexHtml(MissingDir(L"missing")));
    EXPECT_FALSE(HasIndexHtml(L""));

    // 同名目录不算入口文件
    const std::wstring dirNamedIndex = EmptyDir(L"dir-index");
    fs::create_directories(fs::path(dirNamedIndex) / "index.html");
    EXPECT_FALSE(HasIndexHtml(dirNamedIndex));
}

TEST_F(FrontendDirectoryPolicyTest, NoCandidatesResolvesToNone) {
    const auto resolution = Resolve(Candidates{});
    EXPECT_EQ(resolution.source, Source::None);
    EXPECT_TRUE(resolution.directory.empty());
}

TEST_F(FrontendDirectoryPolicyTest, CandidatesWithoutIndexHtmlResolveToNone) {
    Candidates candidates;
    candidates.panelTemplate = EmptyDir(L"panel");
    candidates.activeTemplate = MissingDir(L"active");
    candidates.bundled = EmptyDir(L"dist");
    candidates.defaultTemplate = EmptyDir(L"default");

    const auto resolution = Resolve(candidates);
    EXPECT_EQ(resolution.source, Source::None);
    EXPECT_TRUE(resolution.directory.empty());
}

TEST_F(FrontendDirectoryPolicyTest, EveryLevelWithIndexHtmlPicksPanelTemplate) {
    Candidates candidates;
    candidates.panelTemplate = TemplateDir(L"panel");
    candidates.activeTemplate = TemplateDir(L"active");
    candidates.bundled = TemplateDir(L"dist");
    candidates.defaultTemplate = TemplateDir(L"default");

    const auto resolution = Resolve(candidates);
    EXPECT_EQ(resolution.source, Source::PanelTemplate);
    EXPECT_EQ(resolution.directory, candidates.panelTemplate);
}

TEST_F(FrontendDirectoryPolicyTest, BrokenPanelTemplateFallsBackToActiveTemplate) {
    Candidates candidates;
    candidates.panelTemplate = EmptyDir(L"panel");
    candidates.activeTemplate = TemplateDir(L"active");
    candidates.bundled = TemplateDir(L"dist");

    const auto resolution = Resolve(candidates);
    EXPECT_EQ(resolution.source, Source::ActiveTemplate);
    EXPECT_EQ(resolution.directory, candidates.activeTemplate);
}

TEST_F(FrontendDirectoryPolicyTest, BrokenActiveTemplateFallsBackToBundled) {
    Candidates candidates;
    candidates.activeTemplate = EmptyDir(L"active");
    candidates.bundled = TemplateDir(L"dist");
    candidates.defaultTemplate = TemplateDir(L"default");

    const auto resolution = Resolve(candidates);
    EXPECT_EQ(resolution.source, Source::Bundled);
    EXPECT_EQ(resolution.directory, candidates.bundled);
}

TEST_F(FrontendDirectoryPolicyTest, EmptyBundledDirectoryDoesNotHideDefaultTemplate) {
    // 组件包预建的 dist 目录是空的：它不能挡住后面的 webview-ui\default
    Candidates candidates;
    candidates.activeTemplate = MissingDir(L"active");
    candidates.bundled = EmptyDir(L"dist");
    candidates.defaultTemplate = TemplateDir(L"default");

    const auto resolution = Resolve(candidates);
    EXPECT_EQ(resolution.source, Source::DefaultTemplate);
    EXPECT_EQ(resolution.directory, candidates.defaultTemplate);
}

TEST_F(FrontendDirectoryPolicyTest, UnsetLevelsAreSkipped) {
    Candidates candidates;
    candidates.defaultTemplate = TemplateDir(L"default");

    const auto resolution = Resolve(candidates);
    EXPECT_EQ(resolution.source, Source::DefaultTemplate);
    EXPECT_EQ(resolution.directory, candidates.defaultTemplate);
}
