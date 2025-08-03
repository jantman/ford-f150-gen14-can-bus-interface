# Continuous Integration (CI)

This project uses GitHub Actions for automated testing and building.

## Workflows

### 1. CI Workflow (`.github/workflows/ci.yml`)

**Comprehensive workflow with detailed reporting:**

- **Triggers**: Push and Pull Requests to `main`/`develop` branches
- **Path Filters**: Only runs on changes to:
  - `platformio.ini`
  - `src/**` (source code)  
  - `test/**` (test files)
  - `include/**` (headers)
  - `lib/**` (libraries)

**Jobs:**
- **test-and-build**: Runs native tests and ESP32-S3 build
  - Caches PlatformIO and pip packages for faster builds
  - Runs all 49 native unit tests
  - Builds ESP32-S3 firmware
  - Uploads build artifacts (firmware.bin, firmware.elf)
  - Generates detailed build reports

- **test-coverage**: Analyzes test results and generates coverage reports
  - Provides detailed test statistics
  - Uploads test logs for debugging

### 2. Build and Test Workflow (`.github/workflows/build-test.yml`)

**Simple workflow for basic validation:**

- **Triggers**: Same as CI workflow
- **Path Filters**: Same as CI workflow
- **Jobs**: Single job that runs tests and build quickly

## Local Development

Run the same checks locally:

```bash
# Run all tests (same as CI)
pio test -e native

# Build firmware (same as CI)
pio run -e esp32-s3-devkitc-1

# Run tests with verbose output
pio test -e native -v
```

## Status Badge

The README includes a status badge showing the current CI status:

[![CI](https://github.com/jantman/ford-f150-gen14-can-bus-interface/actions/workflows/ci.yml/badge.svg)](https://github.com/jantman/ford-f150-gen14-can-bus-interface/actions/workflows/ci.yml)

## Test Results

Current test status: **49/49 tests passing (100%)**

Test categories:
- GPIO Controller Tests: 12 tests
- CAN Protocol Tests: 15 tests  
- State Manager Tests: 10 tests
- Integration Tests: 12 tests

## Build Artifacts

The CI workflow automatically uploads build artifacts:
- `firmware.bin` - Flashable ESP32-S3 firmware
- `firmware.elf` - ELF file with debug symbols
- `test-logs` - Detailed test execution logs

Artifacts are retained for 30 days (firmware) and 7 days (logs).
