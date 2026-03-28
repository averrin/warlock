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
    "cmake --build ./" + build_dir + " --target " + target + " --config Debug --parallel 2" \
  } else { \
    "cmake --build ./" + build_dir + " --target " + target + " -j$(nproc 2>/dev/null || echo 4)" \
  } }}

# ── Run ───────────────────────────────────────────────────────────────────────

[no-cd]
run *args: (build bin_name)
  {{ if os_family() == "windows" { \
    "./" + build_dir + "/bin/Debug/" + bin_name + ".exe " + args \
  } else { \
    "./" + build_dir + "/bin/" + bin_name + " " + args \
  } }}

# Restart the running engine process (kill + start in background)
[no-cd]
restart:
  {{ if os_family() == "windows" { \
    "Get-Process -Name " + bin_name + " -ErrorAction SilentlyContinue | Stop-Process -Force; " + \
    "Start-Process -FilePath './" + build_dir + "/bin/Debug/" + bin_name + ".exe' -WorkingDirectory (Get-Location).Path" \
  } else { \
    "pkill -f " + bin_name + " || true; ./" + build_dir + "/bin/" + bin_name + " &" \
  } }}

# Stop, build, and restart the engine
[no-cd]
dev:
  {{ if os_family() == "windows" { \
    "Get-Process -Name " + bin_name + " -ErrorAction SilentlyContinue | Stop-Process -Force; " + \
    "cmake --build ./" + build_dir + " --target " + bin_name + " --config Debug --parallel 2; " + \
    "Start-Process -FilePath './" + build_dir + "/bin/Debug/" + bin_name + ".exe' -WorkingDirectory (Get-Location).Path" \
  } else { \
    "pkill -f " + bin_name + " || true; " + \
    "cmake --build ./" + build_dir + " --target " + bin_name + " -j$(nproc 2>/dev/null || echo 4) && " + \
    "./" + build_dir + "/bin/" + bin_name + " &" \
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

# Run E2E tests (boots headless engine + WebSocket). Optional args forwarded to Catch2 (e.g. "[items]" or a test name substring).
# Batches (tags): session errors frames components connections code environment events concurrency regression items lua state_editor web
# Examples: just test-e2e-batch items   |   just test-e2e -- "[lua]"
[no-cd]
test-e2e *args: (build "warlock_e2e")
  {{ if os_family() == "windows" { \
    "./" + build_dir + "/tests/Debug/warlock_e2e.exe --reporter console " + args \
  } else { \
    "./" + build_dir + "/tests/warlock_e2e --reporter console " + args \
  } }}

[no-cd]
test-e2e-batch batch: (build "warlock_e2e")
  {{ if os_family() == "windows" { \
    "./" + build_dir + "/tests/Debug/warlock_e2e.exe --reporter console '" + "[" + batch + "]" + "'" \
  } else { \
    "./" + build_dir + "/tests/warlock_e2e --reporter console '" + "[" + batch + "]" + "'" \
  } }}

[no-cd]
test-e2e-list: (build "warlock_e2e")
  {{ if os_family() == "windows" { \
    "./" + build_dir + "/tests/Debug/warlock_e2e.exe --list-tests" \
  } else { \
    "./" + build_dir + "/tests/warlock_e2e --list-tests" \
  } }}

[no-cd]
test-e2e-tags: (build "warlock_e2e")
  {{ if os_family() == "windows" { \
    "./" + build_dir + "/tests/Debug/warlock_e2e.exe --list-tags" \
  } else { \
    "./" + build_dir + "/tests/warlock_e2e --list-tags" \
  } }}

# Run all C++ tests
[no-cd]
test: test-unit test-serial test-e2e

# ── Web ──────────────────────────────────────────────────────────────────────

# Install web dependencies
[no-cd]
web-install:
  {{ if os_family() == "windows" { \
    "Push-Location web; npm install; Pop-Location" \
  } else { \
    "cd web && npm install" \
  } }}

# Start web dev server (Vite, port 5173)
[no-cd]
web-dev:
  {{ if os_family() == "windows" { \
    "Push-Location web; npm run dev; Pop-Location" \
  } else { \
    "cd web && npm run dev" \
  } }}

# Build web frontend (TypeScript check + Vite bundle)
[no-cd]
web-build:
  {{ if os_family() == "windows" { \
    "Push-Location web; npm run build; Pop-Location" \
  } else { \
    "cd web && npm run build" \
  } }}

# Run web unit tests (Vitest)
[no-cd]
web-test:
  {{ if os_family() == "windows" { \
    "Push-Location web; npm test; Pop-Location" \
  } else { \
    "cd web && npm test" \
  } }}

# Run web E2E tests (Playwright)
[no-cd]
web-test-e2e:
  {{ if os_family() == "windows" { \
    "Push-Location web; npm run test:e2e; Pop-Location" \
  } else { \
    "cd web && npm run test:e2e" \
  } }}

# Run all tests (C++ + web)
[no-cd]
test-all: test web-test
