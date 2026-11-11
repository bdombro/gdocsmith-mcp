#include "app.hpp"

#include <mcp_http.hpp>

int main(int argc, char* argv[]) {
    auto app = gdocsmith::make_gdocsmith_app();
    slimr::enable_http(app, slimr::HttpServerOptions{.host = "127.0.0.1", .port = 8087});
    return app.run(argc, argv);
}
