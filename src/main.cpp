#include "app.hpp"

#include <mcp_http.hpp>

#include <cstring>
#include <filesystem>
#include <system_error>

/* Entry point. One binary, three surfaces (all share make_gdocsmith_app):
 *   gdocsmith-mcp mcp                         stdio MCP server (NDJSON)
 *   gdocsmith-mcp http [--host --port ...]    Streamable HTTP MCP server at /mcp (default 127.0.0.1:8087)
 *   gdocsmith-mcp <tool> ['{"doc":"..."}']    narrow JSON CLI (export, import, file, status)
 */
int main(int argc, char* argv[]) {
    // Over stdio the server runs in the agent's workspace, so working copies go under <cwd>/.gdocsmith;
    // otherwise (HTTP service, CLI) they live in the private per-user temp root.
    if (argc > 1 && std::strcmp(argv[1], "mcp") == 0) {
        std::error_code ec;
        auto cwd = std::filesystem::current_path(ec);
        if (!ec) gdocsmith::engine::artifactWorkspaceSet(cwd.string());
    }

    auto app = gdocsmith::make_gdocsmith_app();
    slimr::enable_http(app, slimr::HttpServerOptions{.host = "127.0.0.1", .port = 8087});
    return app.run(argc, argv);
}
