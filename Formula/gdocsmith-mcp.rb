class GdocsmithMcp < Formula
  desc "Google Docs file editing and review MCP server"
  homepage "https://github.com/bdombro/gdocsmith-mcp"
  version "4.0.1"

  on_macos do
    url "https://github.com/bdombro/gdocsmith-mcp/releases/download/v4.0.1/gdocsmith-mcp-arm64-macos.tar.gz"
    sha256 "0000000000000000000000000000000000000000000000000000000000000000"
  end

  on_linux do
    url "https://github.com/bdombro/gdocsmith-mcp/releases/download/v4.0.1/gdocsmith-mcp-x64-linux.tar.gz"
    sha256 "0000000000000000000000000000000000000000000000000000000000000000"
  end

  def install
    bin.install "gdocsmith-mcp"
    chmod 0755, bin/"gdocsmith-mcp"
    pkgshare.install "docs" if File.directory?("docs")
  end

  def post_install
    (var/"gdocsmith-mcp").mkpath
    chmod 0700, var/"gdocsmith-mcp"
    (var/"log").mkpath
  end

  service do
    run ["/bin/sh", "-c", "[ -f #{etc}/gdocsmith-mcp/env ] && set -a && . #{etc}/gdocsmith-mcp/env; exec #{opt_bin}/gdocsmith-mcp http --log-file #{var}/log/gdocsmith-mcp.log"]
    keep_alive true
    working_dir var/"gdocsmith-mcp"
    log_path var/"log/gdocsmith-mcp.log"
    error_log_path var/"log/gdocsmith-mcp.log"
    environment_variables PATH: "#{HOMEBREW_PREFIX}/bin:/usr/bin:/bin"
  end

  test do
    assert_match version.to_s, shell_output("#{bin}/gdocsmith-mcp --version")
    discover = '{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}'
    out = pipe_output("#{bin}/gdocsmith-mcp mcp --skip-checks", "#{discover}\n")
    assert_match "gdocsmith-mcp", out
  end
end
