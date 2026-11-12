#pragma once

#include "engine/artifacts.hpp"
#include "test.hpp"

#include <sys/stat.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <string>

namespace gdocsmith::engine::test {

struct TempDir {
    std::filesystem::path path;
    TempDir() {
        auto base = std::filesystem::temp_directory_path() / ("gdocsmith-test-" + detail::unique_tmp_suffix());
        std::filesystem::create_directories(base);
        path = base;
    }
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

TEST_CASE("artifacts - artifactRoot precedence") {
    using namespace gdocsmith::engine;
    TempDir tmp;
    const char* prevEnv = getenv("GDOCSMITH_WORK_DIR");
    std::string prevEnvStr = prevEnv ? prevEnv : "";
    unsetenv("GDOCSMITH_WORK_DIR");

    artifactWorkspaceSet(std::nullopt);
    std::string tempRoot = artifactRoot();
    CHECK(tempRoot.find("gdocsmith-") != std::string::npos);

    // Workspace set
    artifactWorkspaceSet(tmp.path.string());
    std::string wsRoot = artifactRoot();
    CHECK_EQ(wsRoot, std::filesystem::weakly_canonical(tmp.path / ".gdocsmith").string());

    // Environment override
    auto explicitPath = std::filesystem::weakly_canonical(tmp.path / "explicit");
    setenv("GDOCSMITH_WORK_DIR", explicitPath.c_str(), 1);
    CHECK_EQ(artifactRoot(), explicitPath.string());

    // Restore env
    if (!prevEnvStr.empty()) {
        setenv("GDOCSMITH_WORK_DIR", prevEnvStr.c_str(), 1);
    } else {
        unsetenv("GDOCSMITH_WORK_DIR");
    }
    artifactWorkspaceSet(std::nullopt);
}

TEST_CASE("artifacts - workspace is home or root") {
    using namespace gdocsmith::engine;
    unsetenv("GDOCSMITH_WORK_DIR");
    artifactWorkspaceSet("/");
    CHECK(artifactRoot().find("gdocsmith-") != std::string::npos);

    std::string home = detail::get_homedir();
    if (!home.empty()) {
        artifactWorkspaceSet(home);
        CHECK(artifactRoot().find("gdocsmith-") != std::string::npos);
    }
    artifactWorkspaceSet(std::nullopt);
}

TEST_CASE("artifacts - creates private workspace directory that git ignores") {
    using namespace gdocsmith::engine;
    TempDir tmp;
    unsetenv("GDOCSMITH_WORK_DIR");
    artifactWorkspaceSet(tmp.path.string());
    std::string root = artifactRoot();

    struct stat st{};
    CHECK_EQ(stat(root.c_str(), &st), 0);
    CHECK_EQ(st.st_mode & 0777, 0700);

    auto gitignore = std::filesystem::path(root) / ".gitignore";
    CHECK(std::filesystem::exists(gitignore));
    auto content = detail::file_text_read(gitignore.string());
    REQUIRE(content.has_value());
    CHECK_EQ(*content, "*\n");

    struct stat giSt{};
    CHECK_EQ(stat(gitignore.c_str(), &giSt), 0);
    CHECK_EQ(giSt.st_mode & 0777, 0600);

    // Custom .gitignore is kept
    detail::atomic_write(gitignore.string(), "custom\n", 0600);
    artifactRoot();
    auto custom = detail::file_text_read(gitignore.string());
    REQUIRE(custom.has_value());
    CHECK_EQ(*custom, "custom\n");

    artifactWorkspaceSet(std::nullopt);
}

TEST_CASE("artifacts - export folders write export.md and export.orig.md, refreshing re-render but keeping edits") {
    using namespace gdocsmith::engine;
    TempDir tmp;
    std::string root = tmp.path.string();
    std::string fileA = "---\ntitle: A\n---\n::tab{id=t.0 title=Tab}\n\nBody\n";
    auto written = exportFolderWrite(root, "d", "r1", fileA);

    auto dir = tmp.path / "exports" / "d" / "r1";
    CHECK_EQ(written.exportFilePath, (dir / "export.md").string());
    CHECK(std::filesystem::exists(dir / "export.md"));
    CHECK(std::filesystem::exists(dir / "export.orig.md"));

    std::string fileB = "---\ntitle: B\n---\n::tab{id=t.0 title=Tab}\n\nBody\n";
    auto renamed = exportFolderWrite(root, "d", "r1", fileB);
    CHECK(renamed.warnings.empty());
    auto readB = detail::file_text_read(renamed.exportFilePath);
    REQUIRE(readB.has_value());
    CHECK_EQ(*readB, fileB);

    // Now edit export.md locally
    std::string fileAEdited = "---\ntitle: A\n---\n::tab{id=t.0 title=Tab}\n\nEdited\n";
    detail::atomic_write(written.exportFilePath, fileAEdited, 0600);

    auto again = exportFolderWrite(root, "d", "r1", fileB);
    auto readEdited = detail::file_text_read(again.exportFilePath);
    REQUIRE(readEdited.has_value());
    CHECK_EQ(*readEdited, fileAEdited);
    REQUIRE(!again.warnings.empty());
    CHECK(again.warnings[0].find("local edits") != std::string::npos);
}

TEST_CASE("artifacts - read-only export gets timestamped folder without colons") {
    using namespace gdocsmith::engine;
    TempDir tmp;
    std::string fileA = "Doc content";
    auto written = exportFolderWrite(tmp.path.string(), "d", std::nullopt, fileA);
    CHECK(written.exportFilePath.find("no-revision.") != std::string::npos);
    CHECK(written.exportFilePath.find(':') == std::string::npos);
}

TEST_CASE("artifacts - fileReplace permissions") {
    using namespace gdocsmith::engine;
    TempDir tmp;
    auto existing = tmp.path / "existing.md";
    detail::atomic_write(existing.string(), "old", 0640);
    chmod(existing.c_str(), 0640);

    fileReplace(existing.string(), "new");
    struct stat st{};
    stat(existing.c_str(), &st);
    CHECK_EQ(st.st_mode & 0777, 0640);

    auto created = tmp.path / "created.md";
    fileReplace(created.string(), "new");
    stat(created.c_str(), &st);
    CHECK_EQ(st.st_mode & 0777, 0666 & ~detail::current_umask());
}

TEST_CASE("artifacts - inputFileRead and inputFileChanged") {
    using namespace gdocsmith::engine;
    TempDir tmp;
    auto sample = tmp.path / "sample.md";
    detail::atomic_write(sample.string(), "Sample content", 0644);

    auto snap = inputFileRead(sample.string());
    CHECK_EQ(snap.text, "Sample content");
    CHECK(!inputFileChanged(snap));

    detail::atomic_write(sample.string(), "Changed content", 0644);
    CHECK(inputFileChanged(snap));
}

TEST_CASE("artifacts - WriteTracker state transitions") {
    using namespace gdocsmith::engine;
    WriteTracker tracker;
    CHECK_EQ(tracker.writeState(false), "none");
    CHECK_EQ(tracker.writeState(true), "none");

    tracker.writes.push_back(WriteAttempt{.action = "createDocument", .outcome = "pending"});
    CHECK_EQ(tracker.writeState(false), "unknown");

    tracker.writes[0].outcome = "confirmed";
    CHECK_EQ(tracker.writeState(false), "partial");
    CHECK_EQ(tracker.writeState(true), "complete");
}

} // namespace gdocsmith::engine::test
