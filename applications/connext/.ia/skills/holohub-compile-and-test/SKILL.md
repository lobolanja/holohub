---
name: holohub-compile-and-test
description: Technical guide for building and testing Holoscan applications in HoloHub. Covers compilation commands, architecture-specific builds, test execution, and debugging workflows. Use when compiling applications, running tests, or troubleshooting build issues.
---

# HoloHub Compilation and Testing

Technical reference for building and testing Holoscan applications in the HoloHub development environment.

## Contents

- [Quick Reference](#quick-reference) - Essential commands at a glance
- [Build System](#build-system) - Understanding the HoloHub build wrapper
- [Testing](#testing) - Running and debugging tests
- [Architecture-Specific Builds](#architecture-specific-builds) - Cross-compilation and platform targets
- [Troubleshooting](#troubleshooting) - Common issues and solutions

## Quick Reference

### Basic Build Commands

```bash
# Build in debug mode (local development)
./holohub build <application_name> --build-type debug --local

# Build with specific architecture
./holohub build <application_name> --build-type debug --local \
  --configure-args="-DCONNEXTDDS_ARCH=armv8Linux4gcc7.3.0"

# Build with testing enabled
./holohub build <application_name> --build-type debug --local \
  --configure-args="-DBUILD_TESTING:BOOL=ON"

# Full example: Build with architecture and testing
./holohub build connext_ano_basic_app --build-type debug --local \
  --configure-args="-DCONNEXTDDS_ARCH=armv8Linux4gcc7.3.0 -DBUILD_TESTING:BOOL=ON"
```

### Running Tests

```bash
# Navigate to build directory and run all tests
cd /workspace/holohub/build/<application_name>
ctest -V

# Run specific test
cd /workspace/holohub/build/<application_name>
ctest -R <test_name> -V

# Run tests with output on failure only
cd /workspace/holohub/build/<application_name>
ctest --output-on-failure
```

### Build Types

- `debug` - Debug symbols, no optimization, assertions enabled
- `release` - Optimized, no debug symbols
- `relwithdebinfo` - Optimized with debug symbols

## Build System

### HoloHub Wrapper Script

HoloHub uses a wrapper script (`./holohub`) that simplifies CMake-based builds:

```bash
./holohub [command] [application] [options]
```

**Key Commands:**
- `build` - Build an application
- `clear-cache` - Clean build cache
- `list` - List available applications

**Key Options:**
- `--build-type [debug|release|relwithdebinfo]` - Set build configuration
- `--local` - Use local development mode (no Docker)
- `--configure-args="<cmake_args>"` - Pass arguments directly to CMake

### Understanding --configure-args

The `--configure-args` flag allows you to pass CMake variables directly to the build system:

```bash
--configure-args="-DVARIABLE_NAME=value -DANOTHER_VAR=value"
```

**Common CMake Variables:**
- `-DCONNEXTDDS_ARCH=<arch>` - Specify RTI Connext DDS architecture
- `-DBUILD_TESTING:BOOL=ON` - Enable test compilation
- `-DCMAKE_VERBOSE_MAKEFILE:BOOL=ON` - Verbose build output
- `-DCMAKE_BUILD_TYPE=Debug` - Override build type (use --build-type instead)

### Build Directory Structure

```
/workspace/holohub/build/
  └── <application_name>/applications/<application_name>/
      ├── CMakeCache.txt          # CMake configuration cache
      ├── CMakeFiles/             # CMake internal files
      ├── CTestTestfile.cmake     # CTest configuration
      ├── Makefile                # Build system
      ├── <application_binary>    # Compiled application
      └── tests/                  # Test binaries (if enabled)


/workspace/holohub/build/
  └── <operator_name>/operators/<operator_name>/
      ├── CMakeCache.txt          # CMake configuration cache
      ├── CMakeFiles/             # CMake internal files
      ├── CTestTestfile.cmake     # CTest configuration
      ├── Makefile                # Build system
      ├── <operator lib>    # Compiled operator lib/s
      └── tests/                  # Test binaries (if enabled)
```

## Testing

### CTest Basics

CTest is CMake's testing framework. Tests must be enabled at build time with `-DBUILD_TESTING:BOOL=ON`.

**Workflow:**
1. Build with testing enabled
2. Navigate to build directory
3. Run tests with `ctest`

```bash
# Build with tests
./holohub build connext_ano_basic_app --build-type debug --local \
  --configure-args="-DBUILD_TESTING:BOOL=ON"

# Run all tests verbosely
cd /workspace/holohub/build/connext_ano_basic_app
ctest -V
```

### CTest Options

```bash
# Verbose output (shows test stdout/stderr)
ctest -V

# Very verbose (includes test commands)
ctest -VV

# Run specific test by name (regex match)
ctest -R test_name

# Run tests in parallel
ctest -j4

# Stop on first failure
ctest --stop-on-failure

# Show output only for failed tests
ctest --output-on-failure

# Re-run only failed tests
ctest --rerun-failed
```

### Test Development Workflow

1. **Write test** - Add test to CMakeLists.txt
2. **Build with testing** - Enable `-DBUILD_TESTING:BOOL=ON`
3. **Run test** - Use `ctest -R <test_name> -V`
4. **Debug failures** - Check verbose output, use gdb if needed
5. **Iterate** - Rebuild and retest

## Architecture-Specific Builds

### RTI Connext DDS Architectures

When building applications using RTI Connext DDS, specify the target architecture:

**Common Architectures:**
- `x64Linux4gcc7.3.0` - x86_64 Linux with GCC 7.3
- `armv8Linux4gcc7.3.0` - ARM64 Linux with GCC 7.3
- `x64Darwin17clang9.0` - macOS x86_64

**Example:**
```bash
./holohub build connext_ano_basic_app --build-type debug --local \
  --configure-args="-DCONNEXTDDS_ARCH=armv8Linux4gcc7.3.0"
```

### Cross-Compilation

For cross-compilation to different architectures:

1. Ensure cross-compilation toolchain is installed
2. Set `CONNEXTDDS_ARCH` to target architecture
3. CMake will configure for target platform

```bash
# Build for ARM64 target from x86_64 host
./holohub build my_app --build-type release --local \
  --configure-args="-DCONNEXTDDS_ARCH=armv8Linux4gcc7.3.0"
```

## Troubleshooting

### Build Failures

**Issue: CMake cache conflicts**
```bash
# Clear build cache and reconfigure
./holohub clear-cache
./holohub build <app_name> --build-type debug --local
```

**Issue: Missing dependencies**
- Check application's `README.md` for dependencies
- Verify Connext DDS installation if using DDS
- Check `CMakeCache.txt` for found/missing packages

**Issue: Wrong architecture**
```bash
# Verify architecture variable
cd /workspace/holohub/build/<app_name>
grep CONNEXTDDS_ARCH CMakeCache.txt
```

### Test Failures

**Issue: Tests not found**
- Verify `-DBUILD_TESTING:BOOL=ON` was set during build
- Check if `CTestTestfile.cmake` exists in build directory

**Issue: Test binary not found**
```bash
# List available tests
cd /workspace/holohub/build/<app_name>
ctest -N

# Check if test binary exists
ls -la tests/
```

**Issue: Runtime errors**
- Run with verbose: `ctest -VV`
- Check library paths: `echo $LD_LIBRARY_PATH`
- Verify data files are in expected locations

### Debugging Tests

```bash
# Run test binary directly (bypass CTest)
cd /workspace/holohub/build/<app_name>
./tests/<test_binary>

# Use GDB for debugging
cd /workspace/holohub/build/<app_name>
gdb ./tests/<test_binary>
(gdb) run
(gdb) backtrace
```

## Examples

### Complete Build and Test Workflow

```bash
# 1. Clean start
./holohub clear-cache

# 2. Build with all options
./holohub build connext_ano_basic_app \
  --build-type debug \
  --local \
  --configure-args="-DCONNEXTDDS_ARCH=armv8Linux4gcc7.3.0 -DBUILD_TESTING:BOOL=ON"

# 3. Run all tests
cd /workspace/holohub/build/connext_ano_basic_app
ctest -V

# 4. If tests fail, debug
ctest --rerun-failed -VV
```

### Iterative Development

```bash
# Initial build
./holohub build my_app --build-type debug --local --configure-args="-DBUILD_TESTING:BOOL=ON"

# Make code changes...

# Rebuild (incremental)
cd /workspace/holohub/build/my_app
make

# Run specific test
ctest -R my_specific_test -V

# If test fails, run directly for debugging
./tests/my_specific_test
```

### Multi-Architecture Testing

```bash
# Build and test for x86_64
./holohub build my_app --build-type debug --local \
  --configure-args="-DCONNEXTDDS_ARCH=x64Linux4gcc7.3.0 -DBUILD_TESTING:BOOL=ON"
cd /workspace/holohub/build/my_app && ctest -V

# Clean and build for ARM64
./holohub clear-cache
./holohub build my_app --build-type debug --local \
  --configure-args="-DCONNEXTDDS_ARCH=armv8Linux4gcc7.3.0 -DBUILD_TESTING:BOOL=ON"
cd /workspace/holohub/build/my_app && ctest -V
```

## Best Practices

1. **Always use --local in dev container** - Avoids nested Docker containers
2. **Enable testing during development** - Include `-DBUILD_TESTING:BOOL=ON`
3. **Use debug builds for development** - Better error messages and debugging
4. **Run tests frequently** - Catch issues early
5. **Clear cache when switching architectures** - Prevents configuration conflicts
6. **Use verbose test output** - `-V` flag helps diagnose failures
7. **Check build directory size** - Debug builds can be large; clean periodically
8. **Document architecture requirements** - Specify in application README

## Reference

### Environment Variables

```bash
# View current environment
printenv | grep -E 'HOLOSCAN|CONNEXT|CMAKE'

# Common variables
HOLOSCAN_SDK_PATH=/opt/nvidia/holoscan
NDDSHOME=/opt/rti_connext_dds-7.3.0
```

### Directory Navigation

```bash
# Workspace root
cd /workspace/holohub

# Application source
cd /workspace/holohub/applications/<app_name>

# Build directory
cd /workspace/holohub/build/<app_name>

# Test binaries
cd /workspace/holohub/build/<app_name>/tests
```
