---
name: cmake-project
description: "Use when configuring, building, testing, or running a CMake project. Prefer Ninja Multi-Config when Ninja is available, build Release configurations explicitly, and select the newest matching Clang compiler pair on Linux."
user-invocable: false
---

# CMake Project Workflow

Use this workflow whenever a project is built through CMake.

## 1. Inspect the project

- Read the repository's `CMakeLists.txt` and any project-local build instructions before configuring.
- Reuse the repository's existing build directory only when its generator and compiler match the intended configuration. CMake does not allow changing either safely in an existing cache; use a new build directory when they differ.
- Preserve user changes and existing build artifacts. Do not delete or reconfigure an existing build directory just to change generators or compilers.

## 2. Select Clang

Select a matching C and C++ compiler pair before the first configure. Never mix versions, paths, or toolchains between `clang` and `clang++`.

### Linux

Discover these candidates from `PATH`:

- The unversioned pair: `clang` and `clang++`.
- Every versioned pair whose names share the same numeric suffix: `clang-X` and `clang++-X` (for example, `clang-16` and `clang++-16`). Ignore incomplete pairs and non-numeric suffixes.

For each complete pair:

1. Confirm both executables are runnable.
2. Get the actual compiler version from the executable's `--version` output. Do not rank candidates from the filename suffix alone.
3. Compare versions numerically or with a version-aware comparison such as `sort -V`.
4. Choose the pair with the highest actual Clang version. If the unversioned pair ties for the highest version, prefer `clang` and `clang++`; otherwise use the matching highest-suffix pair.

A Bash-compatible discovery outline is:

```sh
candidates=(clang)
for compiler in $(compgen -c clang- | sort -u); do
    case "$compiler" in
    clang-[0-9]*) candidates+=("$compiler") ;;
    esac
done

# For each clang candidate, derive its matching clang++ name, require both
# commands, query --version, and retain the pair with the greatest version.
```

The outline is only a discovery aid: keep the selected compiler paths in variables and pass the same selected pair to CMake. If no complete Clang pair is available, use the project's normal compiler selection rather than inventing a mixed pair.

### Windows

Treat the available `clang` and `clang++` as the single Clang pair. There is no Linux-style suffix ranking requirement on Windows. Use the pair only when both executables are available; otherwise follow the project's normal compiler/toolchain configuration.

## 3. Choose the generator and configure

Check whether Ninja is available before configuring:

```sh
command -v ninja
```

On Windows, use the platform equivalent such as `where ninja`.

When Ninja is available, use the multi-configuration generator exactly as follows:

```sh
cmake -S . -B build -G "Ninja Multi-Config" \
  -DCMAKE_C_COMPILER="$selected_c_compiler" \
  -DCMAKE_CXX_COMPILER="$selected_cxx_compiler" \
  --graphviz=build/dependencies.dot
```

Omit the compiler definitions when no complete Clang pair was selected, allowing CMake or the requested toolchain to choose them. If the project requires a toolchain file or other configure options, retain those options alongside the generator and compiler definitions.

When Ninja is unavailable, use the project's normal generator. Whenever the selected generator is single-configuration, configure Release explicitly:

```sh
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$selected_c_compiler" \
  -DCMAKE_CXX_COMPILER="$selected_cxx_compiler"
```

Adapt quoting and command syntax to the active shell. If `build` has an incompatible existing cache, select a new build directory such as `build-release` rather than changing its generator in place.

## 4. Build Release

Match the build command to the selected generator:

- For `Ninja Multi-Config`, select Release explicitly. Ninja handles its own parallel scheduling:

  ```sh
  cmake --build build --config Release
  ```

- For another multi-configuration generator, such as Visual Studio, select Release and use all available Linux processors:

  ```sh
  cmake --build build --config Release --parallel "$(nproc)"
  ```

- For a single-configuration generator, such as `Unix Makefiles` or `MinGW Makefiles`, do not pass `--config`; Release was selected at configure time. Use all available Linux processors:

  ```sh
  cmake --build build --parallel "$(nproc)"
  ```

On Windows or other platforms without `nproc`, use the platform's processor-count command or pass `--parallel` without a count. Do not add `--config Release` to a single-configuration generator.

## 5. Test or run

After a successful build, use the project's documented test and run commands. For CTest with a multi-configuration build, select Release explicitly:

```sh
ctest --test-dir build -C Release --output-on-failure
```

Report the selected generator, compiler pair, build directory, and configuration when summarizing the result.
