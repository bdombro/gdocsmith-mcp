# Changelog

## Unreleased

- Remove per-tool CLI dispatch and root tool routes; serve tool endpoints under `/tools/<name>` and `/tools/openapi.json` by default

## v4.1.1 - 2026-11-17

- Change default HTTP bind from `0.0.0.0:10007` to `127.0.0.1:10007` (pass `--host 0.0.0.0` to expose) via `slimr-serve` v0.3.0
- Serve OpenAPI 3.1.0 spec at `GET /openapi.json` and `--openapi` CLI flag via `slimr-serve` v0.3.0 / `slimr-mcp` v0.2.1
- Internal: update `scripts/test-mcp.ts` to verify `GET /openapi.json` and document overlay-port dev workflow in `docs/development.md`

## v4.1.0 - 2026-11-16

- Internal: pin `slimr-mcp` v0.2.0 and `slimr-serve` v0.1.1 from `slimr-down-now` registries and remove temporary `overlay-ports`
- Migrate `status` and `export` handlers to `slimr-serve` typed `OutputStruct` schemas (`outputSchema`), and drop `<bin> mcp` stdio mode
- Fix direct CLI tool calls (`<bin> <tool> '{...}'`), disabled by default since `slimr-mcp` v0.1.4
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
