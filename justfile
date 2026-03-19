bin_name := "hellfrost"
build_dir := "build"

# On Windows: use PowerShell for all recipe commands
set windows-shell := ["powershell.exe", "-NoLogo", "-Command"]

bin_ext  := if os_family() == "windows" { ".exe" } else { "" }

# ── Configure ─────────────────────────────────────────────────────────────────

[no-cd]
init:
  {{ if os_family() == "windows" { \
    "if (Test-Path ./" + build_dir + ") { Remove-Item -Recurse -Force ./" + build_dir + " }; " + \
    "cmake -S . -B ./" + build_dir + " -G 'Visual Studio 17 2022' -A x64 '-DCMAKE_POLICY_VERSION_MINIMUM=3.28' '-DSKIP_PERFORMANCE_COMPARISON=ON'; " + \
    "New-Item -ItemType Directory -Force -Path ./" + build_dir + "/bin/Debug | Out-Null; " + \
    "foreach ($d in @('data','scripts','tilesets','fonts', 'assets')) { $t = './" + build_dir + "/bin/Debug/' + $d; if (Test-Path $t) { Remove-Item $t -Force }; New-Item -ItemType Junction -Path $t -Target (Resolve-Path $d).Path | Out-Null }" \
  } else { \
    "rm -rf ./" + build_dir + " && " + \
    "cmake -S . -B ./" + build_dir + " -DCMAKE_BUILD_TYPE=Debug -DCMAKE_POLICY_VERSION_MINIMUM=3.28 -DSKIP_PERFORMANCE_COMPARISON=ON && " + \
    "ln -sf ./" + build_dir + "/compile_commands.json compile_commands.json && " + \
    "mkdir -p ./" + build_dir + "/bin && " + \
    "for d in data scripts tilesets fonts assets; do ln -sfn ../../$d ./" + build_dir + "/bin/$d; done" \
  } }}

# ── Build ─────────────────────────────────────────────────────────────────────

[no-cd]
build target=bin_name:
  {{ if os_family() == "windows" { \
    "cmake --build ./" + build_dir + " --target " + target + " --config Debug" \
  } else { \
    "cmake --build ./" + build_dir + " --target " + target + " -j$(nproc 2>/dev/null || echo 4)" \
  } }}

# ── Run ───────────────────────────────────────────────────────────────────────

[no-cd]
run *args: (build bin_name)
  {{ if os_family() == "windows" { \
    "Copy-Item -Force ./" + build_dir + "/_deps/sfml-build/lib/Debug/*.dll ./" + build_dir + "/bin/Debug/; " + \
    "Copy-Item -Force ./" + build_dir + "/_deps/sfml-src/extlibs/bin/x64/*.dll ./" + build_dir + "/bin/Debug/; " + \
    "./" + build_dir + "/bin/Debug/" + bin_name + ".exe " + args \
  } else { \
    "./" + build_dir + "/bin/" + bin_name + " " + args \
  } }}

# ── Tests ─────────────────────────────────────────────────────────────────────

# Run unit tests (no game engine)
[no-cd]
test-unit: (build "warlock_unit")
  {{ if os_family() == "windows" { \
    "./" + build_dir + "/tests/Debug/warlock_unit.exe --reporter console" \
  } else { \
    "./" + build_dir + "/tests/warlock_unit --reporter console" \
  } }}

# Run serialization tests
[no-cd]
test-serial: (build "warlock_serial")
  {{ if os_family() == "windows" { \
    "./" + build_dir + "/tests/Debug/warlock_serial.exe --reporter console" \
  } else { \
    "./" + build_dir + "/tests/warlock_serial --reporter console" \
  } }}

# Run E2E tests (boots headless engine + WebSocket)
[no-cd]
test-e2e: (build "warlock_e2e")
  {{ if os_family() == "windows" { \
    "Copy-Item -Force ./" + build_dir + "/_deps/sfml-build/lib/Debug/*.dll ./" + build_dir + "/tests/Debug/; " + \
    "Copy-Item -Force ./" + build_dir + "/_deps/sfml-src/extlibs/bin/x64/*.dll ./" + build_dir + "/tests/Debug/; " + \
    "./" + build_dir + "/tests/Debug/warlock_e2e.exe --reporter console" \
  } else { \
    "./" + build_dir + "/tests/warlock_e2e --reporter console" \
  } }}

# Run all tests
[no-cd]
test: test-unit test-serial test-e2e
