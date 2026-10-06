# LLUtils logging rewrite checklist

Updated: 2026-10-04. Runtime and application integration are implemented. Final verification results are recorded below.

[logging.md](logging.md) is the authoritative design, API, default, tradeoff, and validation specification.
This checklist records implementation order and completion gates; it does not duplicate the specification.
Preserve the full selected feature set and D01–D35 decisions. The gated flush policy is defined in the guide.

## 1. Foundation and source boundary

- Confirm the application's pinned LLUtils/LWS revisions and working-tree state before editing.
- Reuse the completed exception diagnostic, formatter, snapshot subscription, bridge lifetime, and platform reporting APIs. Do not introduce another exception dispatcher or terminal-handler owner.
- Modify only the primary LLUtils submodule for LLUtils work; leave nested dependency copies unchanged.
- Preserve unrelated work and use CRLF for created/modified files.

Exit criterion: the exception foundation and existing application tests are available against the selected dependencies.

## 2. Runtime and delivery

- Implement the owned record, ring envelope, configuration snapshot, session lifetime, and single writer described in [ownership and lifecycle](logging.md#runtime-memory-callbacks-and-sinks).
- Implement count/byte admission, complete oversized delivery, ordered barriers/dumps, shutdown wakeups/draining, and reentry/emergency handling.
- Implement writer-owned pending-output-gated flushing; keep no timer while all sinks are clean.

Exit criterion: ordering, independent capacity limits, lifetime, failure cleanup, and gated-flush tests pass.

## 3. Configuration, patterns, and sinks

- Implement permanent categories, filtering, validated text layouts, optional metadata, and captured runtime configuration.
- Implement owned text/raw custom sinks, native UTF-8 boundaries, file rotation, non-inheritable handles, best-effort retention, and sink disabling.
- Explain possible continued disk growth beside retention configuration/API comments and cleanup-error handling.

Exit criterion: format/snapshot tests, raw/text delivery tests, file tests, retention recovery, and child-inheritance checks pass.

## 4. Diagnostic features and application migration

- Implement bounded opt-in history, shared live/replay delivery, fixed startup history format, explicit clearing semantics, and eviction/omission markers.
- Implement operation IDs and explicit request/task/coroutine/completion propagation; preserve generations, stale-result rejection, failed posting, and fatal task discard. No scope may span suspension.
- Implement per-site count/time repetition with suppression counts and session reset.
- Initialize logging after early exits/forwarding and before platform resources; retain it through producer teardown.
- Replace ApplicationLog and the viewer's direct exception observer with the logger-owned observer. Preserve independently requested OIV callbacks and the lazy bridge.
- Migrate startup, renderer/GPU, settings, and image diagnostics while preserving intentional command-line output.
- Reuse platform terminal reporting and connect only the independent preopened emergency destination.

Exit criterion: feature tests and existing exception, bridge, residency, and viewer tests pass without duplicate normal exception output.

## 5. Validation and publication

- Run the guide's [validation contract](logging.md#validation-and-scope), including Windows/Linux integration and C++23 standalone LLUtils coverage.
- Run viewer smoke checks with no input, `External/ImageCodec/Example/cat.jpg`, and `External/ImageCodec/External/FreeImageRe/TestAPI`.
- Measure producer formatting, metadata, snapshots, concurrency, saturation, history, and startup using development tooling only. Do not claim measured guarantees without results.
- Check guide examples against actual APIs and preserve all capability boundaries.
- The authoritative guide is published at repository-root `logging.md`; README has a short Logging overview before License.
- Replace sandbox-only guide links and speculative status with repository-valid references and accurately recorded verification.

Exit criterion: required checks pass or their actual environmental limitations are explicitly recorded; implementation and documentation agree.

## Verification results

- Windows and Linux application builds, existing unit/renderer/font tests, and standalone C++23 logging tests are covered by the validation runs.
- Logging coverage includes filtering, immutable configuration, argument skipping, queue/byte limits, oversized messages, barriers, gated flushing, history, failures, shared exception lifetime, reentry, Unicode paths, retention recovery, and child inheritance.
- Image-residency coverage checks shared decode identity and independent coroutine completion context.
- Native Windows fault subprocesses verify the emergency file without entering normal logging callbacks.
- Startup tools support session-file producer timestamps, so periodic flush latency is excluded from renderer-ready timing.

## Executed verification (2026-10-03)

| Platform | Check | Result |
|---|---|---|
| Windows | `tests_logging` | 34 cases / 826 assertions |
| Windows | `tests` | 225 cases / 6739 assertions |
| Windows | `tests_vk_failures` | 29 cases / 10528 assertions |
| Windows | `tests_font_encoding` | 3 cases / 11 assertions |
| Windows | `tests_vk_runtime_failure` | 3 cases / 14 assertions |
| Linux | `tests_logging` | 34 cases / 826 assertions |
| Linux | `tests` | 227 cases / 6834 assertions |
| Linux | `tests_vk_failures` | 29 cases / 10528 assertions |
| Linux | `tests_font_encoding` | 2 cases / 10 assertions |
| Windows | Exception subprocess checks | 25 passed, including native-fault emergency-file output |
| Linux | Exception subprocess checks | 20 passed |
| Linux / C++23 | Standalone logging suite | 34 cases / 826 assertions passed |
| Python | Startup benchmark regression tests | 7 passed |
| Windows and Linux | No-input, image, folder smoke checks | All six remained running for five seconds |

The Windows image check uses an explicit renderer to prevent forwarding to an existing viewer. Smoke checks prove
process survival; they do not verify displayed pixels. Renderer-ready logging was additionally verified through
session-file producer timestamps on Windows D3D11 and Linux Vulkan. The Linux renderer is llvmpipe/software, so these
runs do not establish hardware presentation. A single startup sample does not establish repeatability.

Optimized development benchmarks cover disabled logging, preformatted/checked formatting, metadata, text rendering,
history, concurrent producers, and atomic shared snapshots. Burst results include queue pressure and scheduling noise;
there is no production latency/throughput guarantee or claimed general speedup. Standard chrono's general formatting
path was measurably expensive, so common layouts use direct chrono arithmetic and reuse output buffers.

The rewrite reduces duplicated paths and transient rendering allocations. The complete implementation remains larger
than a minimal logger because it preserves history, formats, owned sinks, bounded admission, rotation, and lifetime
protection. Additional correctness guards and native failure handling are retained rather than pursuing line count
alone. All selected D01–D35 capabilities and exclusions remain in the guide.

## Test ownership: first migration stage

The newly introduced logging suite and its helper/benchmark now live under `External/LLUtils/Tests/Logging`.
LLUtils owns their CMake definitions, C++23 requirement, pinned standalone Catch2 fallback, and `LLUtils;Logging`
CTest labels. OIViewer enables them through `OIV_BUILD_TESTS` and supplies its Catch2 target. The older exception,
event, string, and application-integration suites remain unchanged in OIViewer for a later migration stage.

Migration validation: the unchanged 34-case logging suite passed via CTest in standalone LLUtils and embedded
OIViewer builds on Windows and Linux. Benchmark targets built explicitly. Library-only configurations on both
platforms registered zero tests and fetched no Catch2; an ordinary embedding defaulted to tests off. OIViewer's
existing suites remained green (225 Windows cases, 227 Linux cases).

## Producer API and constants cleanup

The public preparation/emission pair is replaced by `Logger::Log` text and lazy-factory overloads. Filtering,
repetition, optional capture, active-producer lifetime and admission share one internal operation. Limiter state
is private and identifies each macro site through its compile-time policy; suppressed calls skip metadata capture.
The display-only precision names are `LogTimestampPrecision` and `timestampPrecision`. `LogOptions.h` groups defaults;
local named limits and derived capacities replace coupled literals without adding configuration knobs. Duplicate
emergency notices are shared, and native/private file policies retain their existing behavior.

## Cleanup verification (2026-10-04)

The 38-case logging suite passes all 855 assertions, including an isolated subprocess that verifies the
1,024-byte emergency-output boundary after shutdown. CTest passes for standalone LLUtils and OIViewer-embedded
logging builds on Windows and Linux. Lazy-factory coverage includes filtered consumers, immutable configuration,
repetition, exceptions, reentry, and shutdown waiting for an active factory before rejecting admission.

The existing application suites pass: 225 cases / 6739 assertions on Windows and 227 cases / 6834 assertions on
Linux. Exception subprocess checks pass (25 Windows, 20 Linux). No-input, image and folder smoke checks pass on
both platforms; these prove five-second process survival, not displayed pixels. Linux uses Vulkan software rendering.
Optimized logging benchmarks were executed without claiming a general speedup. Parent and primary LLUtils
working-tree whitespace checks pass.

## Metadata flags and structure documentation cleanup

- Replace metadata masks with shared `BitFlags<LogFields>` and give the enum an explicit `std::uint32_t` underlying type.
- Custom sinks return the wrapper from `RequiredFields()`; remove the redundant membership helper and preserve invalid-bit rejection.
- Document public option contracts and internal ownership, lifetime, synchronization and accounting beside structs/fields.
- Remove unused legacy `LogTarget.h` and `LogPredefined.h` from primary LLUtils only.
- Validate compile-time flag operations, invalid masks, standalone/embedded suites and benchmark builds on both platforms.

Flags/documentation validation (2026-10-04): the updated logging suite passes 39 cases / 859 assertions in
standalone and OIViewer-embedded builds on Windows and Linux. Both benchmark targets and viewer/application-test
targets build successfully. Compile-time checks cover the explicit underlying type and shared flag operations;
runtime coverage rejects unsupported low/high flag bits. CRLF and working-tree whitespace checks pass.
Existing application suites also pass: 225 cases / 6739 assertions on Windows and 227 cases / 6834 assertions on Linux.

## Complete default-policy separation

One `LogDefaults` section precedes the option types in `LogOptions.h`. Every nonempty option policy initializer
references it, as do implicit field padding and the automatic history-replay threshold. The full logging-header
review leaves empty/state initialization, sink interface contracts, native/parser safety bounds, file contracts,
exception event semantics and diagnostic text local. Canonical chrono fast paths stay independent of defaults.
Default values, public option types and aggregate/designated initialization remain unchanged.

Default-policy verification (2026-10-04): the expanded logging suite passes 39 cases / 873 assertions in
standalone and embedded builds on Windows and Linux. Coverage preserves default values, implicit date/time
and padding behavior, custom formats, diagnostic history, and designated option initialization.

## Severity threshold scope names

Rename the option fields to `globalMinimumLevel`, `sinkMinimumLevel` and `historyMinimumLevel`, their default
constants to `GlobalMinimumLevel`, `SinkMinimumLevel` and `HistoryMinimumLevel`, and the filtering setters to
`SetGlobalMinimumLevel`, `SetCategoryMinimumLevel` and `SetSinkMinimumLevel`. Matching private configuration
names and comments describe category replacement, combined live filters, independent history and Off/replay
semantics. Defaults and filtering behavior remain unchanged, with no compatibility aliases.
A two-destination regression verifies global/category replacement, sink filtering, override removal and skipped
lazy factories; existing history/Off/snapshot coverage remains in place.

Scope-name verification (2026-10-04): logging tests pass 40 cases / 901 assertions in standalone and
OIViewer-embedded builds on Windows and Linux. Benchmark targets build in both configurations on both platforms.
Old logging field/setter/default names are absent from primary logging code and consumers. Default values and
history enablement are unchanged; source formatting, CRLF and working-tree whitespace checks pass.

## Reusable mechanics extraction

Refactor baseline: primary LLUtils commit `e7c118c`. Extract `BoundedWorkQueue`, `FileHandle`, `ScopedFileLock` and
`RepetitionLimiter` into LLUtils' public include directory, with no logging dependencies. Extract delivery into
`LoggingDetail::LogWriter`; keep configuration publication and producer/session orchestration in Logger, history
inside the writer, and emergency output on its independent native path. Add LLUtils-owned utility tests using the
existing C++23/Catch2 opt-in. Older upstream tests and nested dependency copies remain untouched.

Extraction verification (2026-10-04): the unchanged logging suite passes 40 cases / 901 assertions. The new
utility suite passes 11 cases / 91 assertions on Windows and 12 cases / 96 assertions on Linux (including
`/dev/full` buffered I/O failures). Both suites pass standalone and embedded; viewer and benchmark targets build
on both platforms. Existing app suites pass 225 Windows cases / 6739 assertions and 227 Linux cases / 6834
assertions. Exception subprocess checks pass (25 Windows, 20 Linux). No-input/image/folder smoke checks passed
on both platforms; these establish process survival, not displayed pixels or hardware Vulkan presentation.

The dequeue result directly owns its payload; a regression asserts exactly one move on dequeue. The common
data-command fast path is retained. Three alternating optimized Windows benchmark runs against the original
baseline produced these median burst wall times (ns/call):

| Case | Before | After |
|---|---:|---:|
| Disabled | 42.0 | 41.0 |
| Preformatted | 257.7 | 229.4 |
| Formatted | 566.4 | 415.0 |
| Metadata | 570.6 | 476.0 |
| Text | 998.4 | 994.4 |
| History | 526.1 | 499.4 |
| Concurrent | 2726.8 | 2500.5 |

Earlier samples were mixed; bursts include queue pressure, scheduling and frequency noise. These results do
not establish a general speedup or latency guarantee. Logger shrinks from 900 to 512 lines and LogSinks from
283 to 187 lines. Total production code grows because reusable ownership interfaces replace embedded mechanics;
this is a readability/reuse refactor, not a claim of net line reduction. Generic utility headers have no logging
dependencies. Formatting, CRLF and parent/primary-submodule whitespace checks pass.

## Emergency output centralization and integration fixes

Centralize file/stderr/debugger fallback output in Emergency::Write, including Unicode console handling.
Replace both platform reporters' direct output with bounded emergency writes while retaining full reports and
platform dialogs. Migrate Main and the exception integration suite to explicit concrete-sink headers, limit
/Zc:preprocessor to compiler ID MSVC, and remove trailing blank lines introduced by test splitting.
Add a hidden Windows console check under code page 437 and a long supplementary-Unicode report check requiring
one complete copy on stderr, in the emergency file and in captured dialogs.

Emergency-centralization verification: standalone and embedded logging/utility CTest suites pass on Windows and
Linux. Windows logging has 40 cases / 902 assertions including the code-page-437 Unicode console check; Linux
retains 40 cases / 901 assertions. Application suites pass (225 Windows cases / 6739 assertions; 227 Linux cases /
6834 assertions). All 26 Windows and 21 Linux exception subprocess checks pass, including complete long Unicode
output in stderr/emergency files, one captured Windows dialog, native faults and allocation-failure paths.
Viewer builds succeed, removed private-write calls are absent, clang-cl no longer receives /Zc:preprocessor,
and formatting/CRLF/working-tree whitespace checks pass.

## Review fixes (2026-10-06)

Both platform exception reporters now include the existing `LLUtils/Emergency.h` and use
`EmergencyDetail::Emergency`. POSIX file segments acquire their protection lock under a temporary
`.opening` name before atomic rename publishes the final segment, closing the creation-versus-retention
window while preserving best-effort retention cleanup. Automatic history triggers capture the fixed
history format's metadata even when the history severity threshold excludes their retention. Unicode
assertions now cover native UTF-16 and UTF-32 units, including UTF-32 surrogate rejection and scalar bounds.
The API documentation names the implemented public history and emergency methods and the actual skip suffix.

Before the fixes, the added history regression reproduced missing timestamp, thread and operation metadata
on Windows. The paused-creation regression reproduced an unlinked active segment on Linux, and the existing
Unicode suite produced seven failing assertions on Linux. After the fixes, both LLUtils C++23 suites pass
through CTest on Windows and Linux. Viewer, application-test and exception-helper targets build on both
platforms. Application tests pass 225 cases / 6739 assertions on Windows and 227 cases / 6834 assertions on
Linux. Exception subprocess checks pass all 26 Windows and 21 Linux scenarios, including the complete long
Unicode report and Windows native faults. All seven Python startup benchmark tests pass. No-input, image
and folder startup checks pass on both platforms, proving five-second process survival without assessing
rendered pixels. Changed C++ files are formatted, changed files retain CRLF, and parent/submodule whitespace
checks pass.
