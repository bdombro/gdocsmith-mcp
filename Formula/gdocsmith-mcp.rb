class GdocsmithMcp < Formula
  desc "Google Docs file editing and review MCP server"
  homepage "https://github.com/bdombro/gdocsmith-mcp"
  url "https://github.com/bdombro/gdocsmith-mcp/releases/download/v4.0.1/gdocsmith-mcp-arm64-macos.tar.gz"
  sha256 "79670f53d0d978d5cfdaf3a6d3a9a648717d62b330f80198e672ff82d92eb7f5"
  version "4.0.1"

  def install
    bin.install "gdocsmith-mcp"
    chmod 0755, bin/"gdocsmith-mcp"
    pkgshare.install "docs" if File.directory?("docs")
  end

  def post_install
    (var/"gdocsmith-mcp").mkpath
    (var/"log").mkpath
  end

  service do
    run [opt_bin/"gdocsmith-mcp", "http", "--host", "127.0.0.1", "--port", "8087"]
    keep_alive true
    working_dir var/"gdocsmith-mcp"
    log_path var/"log/gdocsmith-mcp.log"
    error_log_path var/"log/gdocsmith-mcp.log"
    environment_variables PATH: "#{HOMEBREW_PREFIX}/bin:/usr/bin:/bin"
  end

  test do
    out = shell_output("#{bin}/gdocsmith-mcp status")
    assert_match version.to_s, out
  end
end
