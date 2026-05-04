---
name: oiv-code-writer
description: Write, edit, refactor, format, or review C++ code in the OIViewer/OpenImageViewer repository. Use when Codex works on first-party OIViewer code under OIVShared, OIVAppCore, OIVLib, Clients/OIViewer, or Tests, including feature work, bug fixes, cleanup, platform isolation, and test updates.
---

# OIV Code Writer

Use this skill as the repo-local coding standard for OIViewer C++ work.

## Workflow

1. Treat the repository root as the base for all paths and use repo-relative paths in plans and summaries.
2. Inspect nearby code before editing and match local ownership, naming, and layering.
3. Improve touched code incrementally; avoid broad drive-by rewrites.
4. Format changed C++ files with the repository `.clang-format`.
5. For every added or modified feature, add or update focused tests under `Tests`.
6. Do not reduce existing test coverage unless the user explicitly requests it.
7. Keep line endings consistent with the files being edited and the active repository formatting tools.

## Commit Messages

- When the user asks to commit code, inspect the final diff and status immediately before committing.
- Write a short subject followed by a prose body that highlights the current change.
- Write highlights as complete sentences; separate related highlights with periods or semicolons according to context.
- Do not use bullet lists in commit-message highlights.
- Cover every meaningful changed area in the highlights, including code, tests, config, docs, generated files, and cleanup.
- Avoid vague wording such as `misc fixes` or `updates` when multiple concrete changes exist.

## Module Boundaries

- Use `OIVShared` for general-purpose reusable code.
- Use `OIVAppCore` for reusable app classes, policies, controllers, and helpers.
- Use `Clients/OIViewer` for the viewer application and platform integration.
- Use `OIVLib` for the public OIV API, library internals, and renderer code.
- Treat `External` as external submodules. Do not automatically modify code under `External`; if a requested change appears to belong there, stop and ask before editing.

## Win32 Isolation

- Keep reusable behavior out of `Clients/OIViewer/SrcPlatform/Win32`.
- Prefer moving reusable app decisions into `OIVAppCore`.
- Keep Win32 code focused on OS events, windows, dialogs, timers, clipboard, input, and platform translation.
- When touching `Clients/OIViewer`, prefer refactoring Win32-dependent code behind platform boundaries so future non-Windows clients are not blocked.

## C++ Rules

- Prefer minimal, concise, efficient C++26 over long manual implementations when the shorter form remains readable, debuggable, and locally idiomatic.
- Use advanced C++ and STL utilities to remove real boilerplate, duplication, verbose loops, or error-prone branching; do not use clever constructs that hide intent or fight the surrounding style.
- Prefer structured control flow in ordinary functions. Avoid mid-function `return` for routine branching when `if`/`else`, a result variable, or a scoped branch keeps the code concise and clear.
- Allow early `return` when it materially improves clarity or safety, such as tiny predicate/accessor functions, unrecoverable precondition paths, avoiding excessive nesting, RAII/resource safety, or complete `switch` case handlers.
- Avoid `break` from loops in the same spirit. Prefer loop conditions, sentinel/result variables, extracted predicates, or STL/ranges algorithms when they keep intent clear.
- Allow loop `break` when it is the clearest mechanism, such as search completion, parser/state-machine termination, a `switch` inside a loop, error termination, or performance-sensitive loops where alternatives obscure the code.
- Do not mechanically rewrite existing code solely to remove early exits; apply this guidance to new or touched code when it improves clarity.
- Use C++26 features only when the active build configuration supports them.
- Use designated initializers when the compiler and surrounding code support them cleanly. By default, put each `.field = value` assignment on its own line.
- For arrays or collections of repeated struct entries, each element may put its designated initializer on one line when the repeated pattern is clearer.
- If line breaks would make designated-initializer structure inconsistent across collection entries, format every entry consistently with one line per initialized field.
- Use C++ casts instead of C-style casts.
- Prefer `nullptr` over `NULL`.
- Prefer `const` or `constexpr` over `#define` constants.
- Avoid magic numbers: use a named `const` or `constexpr` value for inline numeric literals whose meaning is not self-evident.
- Keep obvious literals inline when their meaning is clear from the expression or API context.
- Use `enum class` for pure enumerations when ABI or existing public C-style APIs do not require unscoped enums.
- Use `{}` for zero-initialization instead of `memset(0)`.
- Prefer `std::make_unique` and `std::make_shared` over raw `new` when ownership is clear.
- Keep `reinterpret_cast` and `const_cast` at platform, ABI, serialization, or graphics API boundaries; avoid them in ordinary app logic.
- Do not keep redundant helpers. Delete or inline helpers that only forward to another function, wrap a single call without adding policy or validation, preserve an old name after its behavior was removed, have one call site with no abstraction value, or exist only to reduce boilerplate.
- Do not add production functions solely to satisfy tests or expose internals to tests. Test through behavior-facing APIs unless a helper has clear production value.
- Avoid repeated non-trivial code patterns. Prefer a local helper function when duplicated blocks perform the same sequence of operations or checks and the helper can be named after the behavior it provides.

## Testing

- Add or update Catch2 tests in `Tests` for every feature behavior change.
- Prefer tests for app policies, controllers, shared helpers, transforms, sorting, image loading, residency, and formatting behavior.
- Keep platform-specific behavior thin enough that core decisions can be tested without launching the Win32 viewer.
- For bug fixes, add a regression test that fails before the fix when practical.

### OIViewer Smoke Testing

- For changes that affect startup, image loading, folder browsing, file watching, shutdown, or Win32 UI behavior, run the built viewer manually before finishing.
- Prefer the active build output; use `build/codex-ClangCl-22.1/bin/OIViewer.exe` when that build tree is being used.
- Run first with no command-line parameter and verify startup does not crash.
- Run with `External/ImageCodec/Example/cat.jpg` and verify image-load startup does not crash.
- Run with `External/ImageCodec/External/FreeImageRe/TestAPI` and verify folder-load startup does not crash.
- Record whether each run started cleanly. If GUI automation is unavailable, state that the process was launched and whether it exited or crashed unexpectedly.
