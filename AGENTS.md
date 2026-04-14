# Repository Guidelines

## Project Structure & Module Organization
- `core/include/anychat/`: public C++ SDK headers.
- `core/include/anychat_c/`: stable C ABI used by bindings.
- `core/src/`: core implementation (`*_manager.cpp`, networking, DB, C API wrappers).
- `core/tests/`: GoogleTest suites (`test_<module>.cpp`).
- `packages/`: platform bindings (`android`, `ios`, `flutter`, `web`).
- `thirdparty/`: vendored/submodule dependencies (including `glaze`).
- `docs/`, `scripts/`: docs and automation.

## Build, Test, and Development Commands
- `git submodule update --init --recursive`: initialize/update dependencies.
- `cmake -B build -DBUILD_TESTS=ON`: configure project (global C++23 toolchain).
- `cmake --build build -j`: build core libraries and enabled packages.
- `ctest --test-dir build --output-on-failure`: run C++ tests.
- `cmake --preset linux-gcc-debug` (or other presets): reproducible local/CI configs.
- `cd packages/android && ./gradlew test`: Android-side tests.
- `cd packages/flutter && flutter test`: Flutter package tests.

## Coding Style & Naming Conventions
- Formatting: follow `.clang-format` (4 spaces, no tabs, 120 columns).
- Naming: files in `snake_case`; C++ types in `PascalCase`; functions/methods in `lowerCamelCase`.
- C API symbols must keep `anychat_*` prefix and C ABI safety.
- Language policy:
  1. Build standard is **C++23** for all targets.
  2. In first-party business code (`core/`, `packages/`), restrict language usage to **C++17-era features** unless explicitly approved.
  3. New JSON work must use **glaze native type mapping**.
  4. JSON field naming must be **snake_case only** for both serialization and deserialization; do not add camelCase compatibility aliases.
  5. Reuse shared JSON helpers in `core/src/json_common.h` (`ApiEnvelope`, `ApiStatus`, `parseTimestampMs*`, `parseApiEnvelopeResponse`/`parseApiStatus*Response`, `readJsonRelaxed`/`writeJson`, `parseJsonObject`, `decodeJsonObject(string)`, `toLower`/`toLowerCopy`, `parseBoolValue`, `parseInt64Value`, `parseInt32Value`, `pickList`, `nowMs`). Do not duplicate these utilities in manager modules.

## Testing Guidelines
- Framework: GoogleTest (`anychat_core_tests`).
- Add/adjust tests in `core/tests/test_<module>.cpp` with each behavior change.
- Prioritize parsing, state transition, callback, and error-path coverage.
- No fixed global coverage gate; regressions must include focused tests.

## Commit & Pull Request Guidelines
- Prefer Conventional Commits: `feat:`, `fix:`, `refactor:`, optionally scoped (`feat(version): ...`).
- Keep commits atomic and behavior-focused.
- PRs should include:
  1. Problem statement and solution summary.
  2. Linked task/issue.
  3. Build/test evidence (commands + key output).
  4. API/ABI impact notes and any submodule/version updates.
