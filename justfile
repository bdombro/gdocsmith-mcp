set shell := ["zsh", "-f", "-eu", "-o", "pipefail", "-c"]
set positional-arguments

architecture := replace(replace(arch(), "aarch64", "arm64"), "x86_64", "x64")
system := if os() == "macos" { "osx" } else { os() }
triplet := f'{{architecture}}-{{system}}'
compiler := if os() == "macos" { "/usr/bin/clang++" } else { "clang++" }
tap := "bdombro/gdocsmith-mcp"
formula := "gdocsmith-mcp"
endpoint := "http://127.0.0.1:8087/mcp"

_:
    @just --list

deps:
    vcpkg install --x-wait-for-lock --triplet={{triplet}} --x-install-root=build/deps

configure configuration="Debug": deps
    CXX="${CXX:-{{compiler}}}" cmake -S . -B build/cmake/{{configuration}} -G Ninja \
        -DCMAKE_BUILD_TYPE={{configuration}} \
        -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
        -DVCPKG_MANIFEST_MODE=OFF \
        -DVCPKG_INSTALLED_DIR="{{justfile_directory()}}/build/deps" \
        -DVCPKG_TARGET_TRIPLET={{triplet}} \
        -DENABLE_SANITIZERS={{if configuration == "Debug" { "ON" } else { "OFF" }}}

test: (configure "Debug")
    cmake --build build/cmake/Debug --parallel 2
    ctest --test-dir build/cmake/Debug --output-on-failure --no-tests=error

build: (configure "Release")
    cmake --build build/cmake/Release --parallel 2
    ctest --test-dir build/cmake/Release --output-on-failure --no-tests=error

lint: (configure "Debug")
    "${CLANG_TIDY:-clang-tidy}" -p build/cmake/Debug src/main.cpp tests/test_main.cpp

test-mcp: build
    node scripts/test-mcp.mjs build/bin/gdocsmith-mcp export,import,file,status

check-formula:
    ruby -c Formula/{{formula}}.rb

run *args:
    ./build/bin/gdocsmith-mcp "$@"

clean:
    rm -rf build

install: install-local

install-local: build
    #!/usr/bin/env bash
    set -euo pipefail
    brew_prefix="$(brew --prefix)"
    tap_path="$brew_prefix/Library/Taps/bdombro/homebrew-{{formula}}"
    mkdir -p "$(dirname "$tap_path")"
    ln -sfn '{{justfile_directory()}}' "$tap_path"
    trap 'node scripts/dev-formula.mjs reset' EXIT
    node scripts/dev-formula.mjs install
    if brew list --formula "{{tap}}/{{formula}}" >/dev/null 2>&1; then
        HOMEBREW_NO_ASK=1 brew reinstall --formula "{{tap}}/{{formula}}"
    else
        HOMEBREW_NO_ASK=1 brew install --formula "{{tap}}/{{formula}}"
    fi

reinstall-local: build
    #!/usr/bin/env bash
    set -euo pipefail
    was_running=0
    if brew services info "{{formula}}" --json 2>/dev/null | grep -q '"running":[[:space:]]*true'; then
        was_running=1
    fi
    brew_prefix="$(brew --prefix)"
    tap_path="$brew_prefix/Library/Taps/bdombro/homebrew-{{formula}}"
    mkdir -p "$(dirname "$tap_path")"
    ln -sfn '{{justfile_directory()}}' "$tap_path"
    trap 'node scripts/dev-formula.mjs reset' EXIT
    node scripts/dev-formula.mjs install
    HOMEBREW_NO_ASK=1 brew reinstall --formula "{{tap}}/{{formula}}"
    if [[ "$was_running" -eq 1 ]]; then
        just restart
    fi

start:
    #!/usr/bin/env bash
    set -euo pipefail
    brew services start "{{tap}}/{{formula}}"
    just _wait-ready

stop:
    #!/usr/bin/env bash
    set -euo pipefail
    if brew services info "{{formula}}" --json 2>/dev/null | grep -q '"loaded":[[:space:]]*true'; then
        brew services stop "{{tap}}/{{formula}}"
    else
        echo "{{formula}} service is not loaded"
    fi

restart:
    #!/usr/bin/env bash
    set -euo pipefail
    brew services restart "{{tap}}/{{formula}}"
    just _wait-ready

status:
    #!/usr/bin/env bash
    set -euo pipefail
    echo "=== Homebrew service ==="
    if brew list --formula "{{formula}}" >/dev/null 2>&1; then
        brew services info "{{formula}}"
    else
        echo "{{formula}} is not installed via Homebrew"
    fi
    echo "=== HTTP endpoint ({{endpoint}}) ==="
    if curl -fsS -X POST "{{endpoint}}" \
        -H "Content-Type: application/json" \
        -H "Accept: application/json, text/event-stream" \
        -H "MCP-Protocol-Version: 2026-07-28" \
        -H "Mcp-Method: server/discover" \
        -d '{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}'; then
        echo ""
    else
        echo "Endpoint unreachable"
        exit 1
    fi

uninstall:
    #!/usr/bin/env bash
    set -euo pipefail
    if brew services info "{{formula}}" --json 2>/dev/null | grep -q '"loaded":[[:space:]]*true'; then
        brew services stop "{{tap}}/{{formula}}"
    fi
    if brew list --formula "{{tap}}/{{formula}}" >/dev/null 2>&1; then
        HOMEBREW_NO_ASK=1 brew uninstall --formula "{{tap}}/{{formula}}"
    elif brew list --formula "{{formula}}" >/dev/null 2>&1; then
        HOMEBREW_NO_ASK=1 brew uninstall --formula "{{formula}}"
    fi
    if brew tap | grep -qx "{{tap}}"; then
        HOMEBREW_NO_ASK=1 brew untap "{{tap}}"
    fi

_wait-ready:
    #!/usr/bin/env bash
    set -euo pipefail
    for _ in {1..30}; do
        if curl -fsS -X POST "{{endpoint}}" \
            -H "Content-Type: application/json" \
            -H "Accept: application/json, text/event-stream" \
            -H "MCP-Protocol-Version: 2026-07-28" \
            -H "Mcp-Method: server/discover" \
            -d '{"jsonrpc":"2.0","id":1,"method":"server/discover","params":{"_meta":{"io.modelcontextprotocol/protocolVersion":"2026-07-28","io.modelcontextprotocol/clientCapabilities":{}}}}' >/dev/null 2>&1; then
            echo "Ready at {{endpoint}}"
            exit 0
        fi
        sleep 0.2
    done
    echo "Timed out waiting for {{endpoint}}" >&2
    exit 1
