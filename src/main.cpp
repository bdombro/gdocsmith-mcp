#include "app.hpp"
#include "log_file.hpp"

#include <mcp_http.hpp>

#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

/* Entry point (shares make_gdocsmith_app):
 *   gdocsmith-mcp http [--host --port ...]    Streamable HTTP MCP (/mcp) and /tools/<name> server (default 127.0.0.1:10007)
 */
int main(int argc, char *argv[]) {
    std::optional<std::string> logFile;
    std::vector<char *> filteredArgv;
    if (argc > 0 && argv != nullptr) {
        filteredArgv.reserve(static_cast<size_t>(argc));
        filteredArgv.push_back(argv[0]);
        for (int i = 1; i < argc; ++i) {
            const char *arg = argv[i];
            if (arg == nullptr)
                continue;
            std::string_view sv(arg);
            if (sv == "--log-file") {
                if (i + 1 >= argc || argv[i + 1] == nullptr || argv[i + 1][0] == '\0') {
                    std::cerr << "Missing path for --log-file\n";
                    return 1;
                }
                logFile = argv[++i];
            } else if (sv.starts_with("--log-file=")) {
                std::string_view val = sv.substr(std::string_view("--log-file=").size());
                if (val.empty()) {
                    std::cerr << "Missing path for --log-file\n";
                    return 1;
                }
                logFile = std::string(val);
            } else {
                filteredArgv.push_back(argv[i]);
            }
        }
    }

    std::unique_ptr<app_log::LogFileGuard> logGuard;
    if (logFile.has_value()) {
        auto opened = app_log::LogFileGuard::open(*logFile);
        if (!opened) {
            std::cerr << opened.error() << "\n";
            return 1;
        }
        logGuard = std::move(*opened);
    }

    auto app = gdocsmith::make_gdocsmith_app();
    slimr::enable_http(app, slimr::HttpServerOptions{.port = 10007});
    return app.run(static_cast<int>(filteredArgv.size()), filteredArgv.data());
}
