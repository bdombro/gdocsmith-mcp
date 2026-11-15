// Standalone negative-control runner. Compile separately, never into test_main.
// Each mode runs the real fixture test against a disposable isolated corpus.
#define DOCTEST_CONFIG_IMPLEMENT
#include "test_conformance.hpp"

#include <iostream>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "usage: conformance_control valid|malformed|missing|unreadable|wrong\n";
        return 2;
    }
    const std::string mode(argv[1]);
    if (mode != "valid" && mode != "malformed" && mode != "missing" &&
        mode != "unreadable" && mode != "wrong")
        return 2;

    const auto original = std::filesystem::current_path();
    slimr::testing::TempDirGuard workspace("conformance_control_");
    // Restore the working directory before removing the temporary corpus.
    struct RestoreDirectory {
        std::filesystem::path path;
        ~RestoreDirectory() {
            std::error_code error;
            std::filesystem::current_path(path, error);
        }
    } restore{original};

    try {
        const auto fixture_dir = find_fixture_dir();
        if (fixture_dir.empty())
            throw std::runtime_error("control source fixture directory missing");
        const auto source = std::filesystem::absolute(fixture_dir);
        const auto isolated = workspace.path / "tests/fixtures/conformance";
        std::filesystem::create_directories(isolated);
        for (const auto &entry : std::filesystem::directory_iterator(source)) {
            if (entry.path().filename() != "B1.json") {
                std::filesystem::create_symlink(entry.path(), isolated / entry.path().filename());
            }
        }
        const auto selected = isolated / "B1.json";
        if (mode == "valid") {
            std::filesystem::create_symlink(source / "B1.json", selected);
        } else if (mode == "missing") {
            std::filesystem::create_symlink(isolated / "absent.json", selected);
        } else if (mode == "unreadable") {
            std::filesystem::copy_file(source / "B1.json", selected);
            std::filesystem::permissions(selected, std::filesystem::perms::none);
        } else {
            std::ofstream output(selected);
            output.exceptions(std::ios::badbit | std::ios::failbit);
            if (mode == "wrong") {
                std::ifstream input(source / "B1.json");
                const std::string text((std::istreambuf_iterator<char>(input)), {});
                auto json = slimr::parse_json<slimr::Json>(text);
                if (!json)
                    throw std::runtime_error("control source fixture did not parse");
                (*json)["replies"] = slimr::Json::array_t{};
                output << slimr::write_json(*json);
            } else {
                output << "{";
            }
            output.close();
        }
        std::filesystem::current_path(workspace.path);
        std::cout << "isolated control: " << mode << '\n';
        doctest::Context context;
        context.setOption("test-case", "fixtures exist and pass conformance");
        return context.run();
    } catch (const std::exception &error) {
        std::cerr << "control setup failed: " << error.what() << '\n';
        return 2;
    }
}
