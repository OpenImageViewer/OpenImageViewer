# Image pipeline migration

Working migration with incomplete validation, implemented from baseline `9b99d35ed50ac40b639e94497a75dfedb201cddb`.
LLUtils remains at the committed pin `79fd25fc57c1506a97b9b65362658340efba37e3`.
External source and the C command structures have not changed.

## Ownership and interfaces

| Component | Ownership and responsibility |
| --- | --- |
| ViewerImage | Opened CPU pixels, original metadata, source and normalized file path |
| ImageState | Selected source, four cached processing results, view state, CPU resampler and generation-based color statistics |
| TextRasterizer | Text options, independent metric/bitmap invalidation and cached CPU bitmap |
| ViewerApplication | Two optional main-image RenderImage owners, independent autoscroll presentation and actual-frame scheduling |
| TextLabel / LabelManager | Lazy text preparation and registrations created in label creation order |
| Renderer | Stable facade, common CPU records, renderer-local IDs and replacement-only content revisions |
| IRenderBackend | Selected graphics implementation, lazy GPU resources, upload acknowledgment and retirement |

The old OIVBaseImage hierarchy, ImageChain, IRenderable and IRenderer have been removed.
RendererCore has no dependency on a concrete backend, the viewer or the full oiv target.
File/raw creation and reusable resampling live below OIVAppCore and require no API instance.

ImageState accessors only observe cached results. Explicit Refresh prepares dependencies up to
its requested stage, preserving the resampling preference and threshold. Invalidated results
release their CPU ownership immediately. Geometry is explicitly prepared before placement.
Workers retain CPU pixels and a captured source generation, never a presentation.

UpdateMainRenderImages runs after model changes and before actual presentation. Immediate,
timer and deferred presentation share PresentPreparedFrame. Resuming a previously approved
deferred frame prepares it directly, preserving its existing scheduling decision.

Labels receive empty CPU registrations in creation order when the renderer becomes available.
New labels register immediately afterward. Hidden labels can measure without rasterizing.
Property changes reuse pixels. Successful empty output clears old content without changing
registration order, and preparation exceptions retain dirty work for retry.

RenderFrame borrows the common map only during Render. Backend caches retain no record pointers.
Replacing pixels advances content revision, while unchanged pixels and properties do not.
Vulkan acknowledges uploads after successful submission. OpenGL validates actual uploads before
acknowledgment. Failed Vulkan release retains retired resources in the existing cache entry until
completion or backend destruction. RenderImage destruction and backend release are nonthrowing.

Shutdown invalidates UI callback lifetime and detaches message expiry/fade producers before releasing labels,
the autoscroll anchor and both main-image owners. The stable facade survives native shutdown until
final API destruction, and queued CPU results require no final callback to release their data.

## Verification

Windows uses the existing `build/ClangCl-22.1/Debug` tree with Clang 22.1.8.
Linux uses `build/linux-clang-debug` with Clang 21.1.7 and the configured Wayland backend.
The Linux tree reuses the existing pinned GLEW source cache to avoid a WSL extraction rename failure.

Baseline Windows suites passed with 229 unit cases, 29 Vulkan failure cases, three font cases
and three missing-runtime cases. Final Windows all-renderer coverage passes 237 unit cases.
Final Linux default coverage passes 238 unit cases, including actual OpenGL pixel replacement.
Vulkan failure coverage passes 33 cases on both platforms. Font and Windows missing-runtime
suites pass, and explicit native renderer shutdown passes on both platforms.

Windows D3D11, Vulkan and OpenGL startup, browsing, window transitions and orderly shutdown pass.
Linux Vulkan and OpenGL empty/image/folder startup passes. Private Wayland compositor loss reports
the failure and cleans up without a crash. The production queued-callback lifetime regression passes.

Standalone backend and test-only Null configuration results are recorded in the build matrix logs.
Default renderer configurations are restored after these checks.

## Remaining findings and validation limits

Successful suite runs above are individual observations, not a stability guarantee.
The Windows unit suite has also intermittently failed with heap corruption. Debugger runs
confirmed modification of a freed heap block, but the source of the invalid write remains
unidentified. The AddressSanitizer diagnostic build failed in LLVM linking with an associative
COMDAT error, so it has not supplied a memory-safety result.

Review confirmed that Vulkan can draw a previously uploaded texture after empty pixels are
published. The empty-content test currently checks uploads rather than draw suppression.
The inherited CPU resampler also indexes source rows by logical width rather than row pitch,
which misreads valid padded RGBA input. Both findings still require fixes and focused regressions.
The complete migration should not be treated as ready for release until these issues are resolved.

## Runtime cost accounting

Processing stages no longer allocate image wrappers or renderer registrations. GPU allocation is
lazy, frame traversal borrows records without allocating a snapshot, and unchanged synchronization
compares shared-image pointers without copying ownership. Release and frame debug contracts add
no new facade locks, and the frame mutation guard is omitted in optimized builds.

Each presentation now owns a common map record in addition to its lazy backend cache entry.
Backend frame traversal adds ID-based cache lookups. LabelManager retains a creation-order vector.
OpenGL checks errors only after actual texture uploads. These structural costs are explicit.
No comparable optimized runtime trace was collected, so this migration makes no speedup claim
and does not establish performance equivalence for every workload.
