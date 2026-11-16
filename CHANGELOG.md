# Changelog

## Unreleased

- Internal: replace `slimr-curl` with `cpp-httplib` (`httplib::Client`) and upgrade `slimr-mcp` to v0.1.3 / `slimr-bugsink` to v0.1.2

- Internal: switch macOS builds to llvm@23 with static libc++ and hidden symbol exports
- Embed VERSION into binary (--version / status) and tag Bugsink events with gdocsmith-mcp@<version>
- Bind HTTP server to 0.0.0.0:10007 by default (pass --host 127.0.0.1 for loopback-only)
- Move default non-workspace work directory from $TMPDIR to $(brew --prefix)/var/gdocsmith-mcp (mode 0700)
- Add --log-file <path> option with 10 MiB log rotation to <path>.1 (mode 0600)
- Support --skip-checks before or after the mcp subcommand
- Internal: align build, CI, Homebrew formula, and documentation with cpp-templates
- Internal: resolve slimr registries from the `slimr-down-now` org
- Internal: fix Clang 22+ doctest anonymous counter collisions and raise ctest timeout to 300s
