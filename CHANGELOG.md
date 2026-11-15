# Changelog

## Unreleased

- Move default non-workspace work directory from $TMPDIR to $(brew --prefix)/var/gdocsmith-mcp (mode 0700)
- Add --log-file <path> option with 10 MiB log rotation to <path>.1 (mode 0600)
- Support --skip-checks before or after the mcp subcommand
- Internal: align build, CI, Homebrew formula, and documentation with cpp-templates
