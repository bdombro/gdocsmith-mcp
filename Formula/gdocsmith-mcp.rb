class GdocsmithMcp < Formula
  desc "Google Docs file editing and review MCP server"
  homepage "https://github.com/bdombro/gdocsmith-mcp"
  version "4.1.2"

  on_macos do
    url "https://github.com/bdombro/gdocsmith-mcp/releases/download/v4.1.2/gdocsmith-mcp-arm64-macos.tar.gz"
    sha256 "e2c3e7343ee6c77bca97aad172775ab192d0c08e5dda7554fdaa6702602bc3c8"
  end

  on_linux do
    url "https://github.com/bdombro/gdocsmith-mcp/releases/download/v4.1.2/gdocsmith-mcp-x64-linux.tar.gz"
    sha256 "8c6c99c905cb5132099296d437083050120e1f29221dd64ec2fc12fccf35bc1c"
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
    require "net/http"
    assert_match version.to_s, shell_output("#{bin}/gdocsmith-mcp --version")
    r, w = IO.pipe
    pid = spawn("#{bin}/gdocsmith-mcp", "--skip-checks", "http", "--host", "127.0.0.1", "--port", "0", err: w)
    w.close
    begin
      port = nil
      r.each_line { |line| break if (port = line[%r{listening on http://[\d.]+:(\d+)/mcp}, 1]) }
      assert port, "server did not report a listening port"
      discover = '{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}'
      res = Net::HTTP.post(URI("http://127.0.0.1:#{port}/mcp"), discover,
                           "Content-Type"         => "application/json",
                           "Accept"               => "application/json, text/event-stream",
                           "MCP-Protocol-Version" => "2026-07-28",
                           "Mcp-Method"           => "server/discover")
      assert_match "gdocsmith-mcp", res.body
    ensure
      Process.kill("TERM", pid)
      Process.wait(pid)
    end
  end
end
