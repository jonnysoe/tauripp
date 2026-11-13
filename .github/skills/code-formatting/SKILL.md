---
name: code-formatting
description: "Use when formatting this CMake C/C++ project, running clang-format, updating the format target, or verifying formatting exclusions."
---

# Code Formatting

Run the repository's CMake formatting target from the project root:

```sh
cmake --build build --config Release --target format
```

Do not invoke `clang-format` or `cmake/clang_format.cmake` directly. The target handles file discovery and excludes `build`, `third_party`, `temp`, and `tmp` directories.
