---
name: oiv-code-writer
description: Write, edit, refactor, format, or review C++ code in the OIViewer/OpenImageViewer repository. Use when Codex works on first-party OIViewer code under OIVShared, OIVAppCore, OIVLib, Clients/OIViewer, or Tests, including feature work, bug fixes, cleanup, platform isolation, and test updates.
---

# OIV Code Writer

Use this skill as the repo-local coding standard for OIViewer C++ work.

## Workflow

1. Treat the repository root as the base for all paths and use repo-relative paths in plans and summaries.
2. Inspect nearby code before editing and match local ownership, naming, and layering.
3. Actively inspect new and modified code for clearer modern C++/STL expressions and opportunities to move computation to compile time. Keep improvements local to the requested work; avoid broad drive-by rewrites.
4. Format changed C++ files with the repository `.clang-format`.
5. Add or update focused tests under `Tests` when changed behavior is not already proven by existing coverage or enforced compile-time contracts.
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

## Context First

- Before modifying code, read nearby comments, callers, tests, ownership rules, and platform or framework contracts. Preserve the assumptions and supported-use boundaries they establish.
- Optimize for the actual inputs and use cases in scope rather than every theoretically possible case.
- Express real compile-time constraints through types, narrow interfaces, and C++ concepts or `requires` in generic code. Do not introduce templates or abstractions solely to encode context for concrete code.

## C++ Rules

- Prefer the simplest direct implementation that satisfies the current request. Introduce a new abstraction or architectural layer only when the user asks for it or a concrete current requirement—such as repeated logic, ownership, or a platform boundary—needs it; do not generalize for hypothetical future use.
- Write concise, minimal, and readily understandable C++26 code containing only what the request and current behavior require. Treat every added branch, check, copy, allocation, conversion, abstraction, and state transition as work that must justify its cost.
- Keep new public and internal API surface to the minimum required for the requested functionality: use the fewest necessary types, functions, methods, parameters, overloads, callbacks, and configuration options, and keep implementation details private.
- Prefer one focused, concise interface when it can provide the required functionality. Do not add convenience variants, extensibility points, or generalized hooks for hypothetical callers; preserve existing APIs unless the task explicitly authorizes their removal.
- Add only code required by current behavior. Remove includes, fields, functions, branches, and other state made unused by the change.
- Actively seek modern C++ language features, keywords, standard algorithms, ranges, and utilities that express intent more directly and remove boilerplate in new and modified code. Apply this to runtime code as well as compile-time code.
- Replace manual loops, searches, branching, and repetitive operations when the standard facility makes the code clearer and simpler while preserving behavior and performance. Prefer meaningful simplification over fewer lines; avoid clever constructs that hide intent or fight the surrounding style.
- Prefer a single exit point for simple, routine control flow. Structure the function with `if`/`else`, scoped branches, or a result variable, and place its `return` at the end so readers can identify the exit without searching for mid-function returns.
- Reserve early `return` for exceptional or unrecoverable paths, or cases where a single exit would materially reduce clarity or safety, such as excessive nesting, RAII/resource safety, or complete `switch` case handlers. Do not use early returns for ordinary branching merely because they shorten the function.
- Avoid `break` from loops in the same spirit. Prefer loop conditions, sentinel/result variables, extracted predicates, or STL/ranges algorithms when they keep intent clear.
- Allow loop `break` when it is the clearest mechanism, such as search completion, parser/state-machine termination, a `switch` inside a loop, error termination, or performance-sensitive loops where alternatives obscure the code.
- Do not mechanically rewrite existing code solely to remove early exits; apply this guidance to new or touched code when it improves clarity.
- Use modern language and standard-library features, including C++26 features, only when the active compiler, standard library, and build configuration support them.
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
- Before implementing helper or utility logic, search the relevant first-party modules for equivalent functionality. Reuse or appropriately extend an existing helper when its behavior, ownership, layering, and performance fit; do not duplicate the same implementation.
- When no suitable helper exists, prefer a local helper only when repeated non-trivial blocks perform the same sequence of operations or checks and the helper can be named after the behavior it provides.

## Compile-Time Evaluation

- Actively identify computations, constants, tables, and decisions in new and modified code whose required inputs and operations permit compile-time evaluation. Perform eligible work at compile time; retain runtime execution for runtime-dependent work.
- Prefer `consteval` for functions when all intended calls can and should execute at compile time.
- Use `constexpr` when the same operation needs both compile-time and runtime use. Preserve existing runtime-capable contracts.
- Where compile-time evaluation must be guaranteed, use a constant-expression context such as a `constexpr` initializer or `static_assert`. Declaring a function `constexpr` alone does not guarantee compile-time execution.
- Use `if constexpr` for appropriate compile-time-dependent branches. Keep improvements local and simple; do not introduce templates, abstractions, or duplicated implementations solely to force compile-time evaluation.

## Performance

- Treat runtime performance as a first-class acceptance criterion, not a secondary cleanup concern.
- Review each change for added runtime work, especially copies, allocations, conversions, synchronization, repeated checks, and work inside per-frame, per-image, per-item, or input-event paths.
- Do not introduce a known or plausible runtime regression without the user's explicit permission, even when the added work improves correctness, safety, validation, diagnostics, debuggability, testability, readability, or generality.
- When correctness or safety conflicts with performance, stop before implementing the slower option. Explain the concrete risk, affected path, expected cost, and available zero- or lower-overhead alternatives, then request permission for any remaining regression.
- Prefer compile-time enforcement, trusted internal contracts, validation at existing external boundaries, cached or amortized work, and other zero-overhead mechanisms over repeated runtime checks. Do not remove existing validation or safety checks unless the requested change authorizes it.
- When workload context is insufficient to rule out a regression, flag the affected path, suspected cost, and evidence or measurement needed; do not assume the added cost is acceptable.

## Documentation and Exceptional Handling

- For new or changed first-party exception paths, prefer the LLUtils exception facilities, such as `LL_EXCEPTION`, `LL_EXCEPTION_SYSTEM_ERROR`, `LL_EXCEPTION_UNEXPECTED_VALUE`, and `LL_EXCEPTION_NOT_IMPLEMENT`, over directly throwing standard-library exception types.
- Choose the narrowest meaningful `LLUtils::Exception::ErrorCode`; use `LL_EXCEPTION_DONT_THROW` only when reporting an error without unwinding is the intended behavior.
- Keep a standard-library or foreign exception when an API, framework, test, or third-party boundary requires that exact exception type, or when reaching LLUtils would introduce a new or cumbersome dependency. Catching `std::exception` at a boundary remains appropriate when failures from standard-library or external code must also be handled.
- Do not add an LLUtils dependency solely to replace a standard exception without first checking the target's existing dependency graph and nearby conventions.
- Prefer expressive names, narrow types, and clear structure. Use comments to preserve information that the code cannot make obvious, not to compensate for unclear code.
- Document a function contract when callers cannot readily infer it from the signature and implementation. Capture the relevant input assumptions, supported-use boundaries, invariants, ownership and lifetime, side effects, ordering or threading requirements, platform assumptions, and performance decisions. Do not require Doxygen or a comment for every function.
- Explain non-obvious logic and design choices at the narrowest relevant scope. Connect the reason for a choice to the behavior or invariant it preserves, especially when a simpler-looking alternative would be incorrect.
- When an implementation is intentionally limited, partial, or scoped to current use cases, document that design decision at the narrowest relevant scope. State the supported scope, what a full or more comprehensive implementation would require, and why that work is deferred or not justified now.
- Record surprising user-visible precedence, cancellation, or interaction rules beside the decision that enforces them, and encode the same contract in focused tests when practical.
- Document workarounds and exceptional platform or framework handling with the external constraint that requires them and a removal condition when useful.
- When nearby behavior changes, revalidate its comments and update or remove anything stale, contradictory, or no longer useful.
- Do not narrate obvious statements, branches, or control flow.

## Testing

- Add or update Catch2 tests when behavior changes and existing coverage does not prove it.
- Omit a test or runtime check only when repository evidence shows an enforced contract makes the case impossible or existing tests already cover it. Document any non-obvious invariant used to justify the omission, and preserve checks at external, trust, indexing, ownership, and ABI boundaries.
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
