---
name: project-local-temp
description: "Use when creating scratch pads, temporary files or directories, exploratory scripts, logs, generated artifacts, downloaded files, screenshots, or other disposable work. Keep those artifacts inside an ignored, project-local throwaway directory such as ./temp/ or ./tmp/."
user-invocable: false
---

# Project-Local Temporary Work

Keep disposable work inside the project so it is easy to inspect, reproducible in context, and covered by the repository's ignore rules.

## Procedure

1. Inspect the repository's `.gitignore` before choosing a location.
2. Prefer an existing ignored directory whose name clearly indicates temporary work, such as `./temp/` or `./tmp/`. In this repository, use `./temp/`.
3. If no suitable directory exists, use `./temp/` and add `temp/` to `.gitignore` before creating artifacts there.
4. Put scratch pads, intermediate files, command output, generated assets, exploratory scripts, and downloaded inputs under that directory. Create a task-specific subdirectory when several artifacts are needed.
5. Use repository-local paths in commands and scripts, for example `./temp/inspect/output.txt`.
6. Do not use `/tmp`, `/var/tmp`, the user's home directory, or another external location for task artifacts unless the tool or operating system requires it. If an external tool forces such a path, copy the result into the project-local temporary directory as soon as practical.
7. Before finishing, remove unnecessary temporary artifacts or leave them only in the ignored directory when they are useful for reproducing or reviewing the work.

## Scope

This rule applies to temporary files and exploratory work, not to normal source files, documentation, tracked test fixtures, or build directories that the project already manages explicitly.