# D3D12 capture changes and validation

Validated 2026-10-01 through 2026-10-03; scope confirmed 2026-10-04.

## Changes

- Snapshot referenced unmapped UPLOAD buffers at submission, extending the existing GPU_UPLOAD path so replay receives CPU data used by PRAGMATA.
- Order captured queue operations around forward fence dependencies, retaining CPU-write chronology and per-queue order. Fall back to original order for dependencies that cannot be ordered. Include queue-order unit cases.
- Submit and wait for bounded initial-copy batches during capture. Do not reset allocators or recycle submitted lists after a failed GPU wait.
- Remove released resource IDs from bindless frame references under the existing resource-state lock to avoid accumulating historical allocations.
- Preserve native Agility factory interfaces through nested hooks and balance DRED settings references.
- Add opt-in initial-copy/DRED/UAV diagnostics and thread-safe NVIDIA interface logging. Report actual Cubin launches; Cubin work itself is not serialized.

## Reproduction

Use `x64/FlushFailureCandidate/qrenderdoc.exe`, standard Launch Application, the game's executable and working directory, empty arguments/environment overrides, and F12 after entering gameplay. Disable DLSS and frame generation. TLOU2 used FSR 3.1.3. No capture DRED or per-resource diagnostic overrides were enabled for the default validation below. Close the game before independent GPU replay. Ensure enough free space in the capture output directory; TLOU2 produced a 9.41 GB file.

Base commit: `5e1d245c3923860372b4fa820b058bbd9015ce41`.
Tested `rdoc.dll` SHA256: `C21A7B359BDD283C92BA480D63AA35ACD33FF2E9916F817BBF7A2E732428EA1A`.
No source or binary changes were made after these runs; final edits only reconcile documentation.

## Results

All rows used standard GUI Launch and gameplay F12. Games survived capture. Each resulting file passed OpenFile, OpenCapture, SaveTexture and FatalErrorStatus checks; exported final images were visually inspected against the live scene.

| Game | Frame | Capture bytes | Replay actions / Present event |
| --- | ---: | ---: | --- |
| CONTROL Resonant | 67647 | 4903330735 | 1931 / 22556 |
| CONTROL Resonant, second F12 in same process | 69203 | 4879867753 | 1935 / 22585 |
| Control | 38898 | 2588287987 | 2971 / 35628 |
| PRAGMATA | 30795 | 4460604289 | 3283 / 13276 |
| FINAL FANTASY XVI | 54441 | 4781098346 | 7676 / 55807 |
| Onimusha | 50664 | 2700885430 | 3905 / 19551 |
| The Last of Us Part II | 2750900 | 9411984484 | 4602 / 105025 |

Current-runtime D3D/D3D12 tests passed: 14,945 assertions in 7 cases, exit 0. `git diff --check` passed. An existing FFXVI capture also replayed successfully; 99.968669% of exported pixels matched its earlier baseline, with maximum channel difference 4/255.

Local evidence (large captures and machine logs are intentionally not committed): `RDC/*flushfailurecandidate*`, `RDC/flushfailurecandidate_final_d3d_tests.txt`, and `CAPTURE_VALIDATION_MANIFEST.json`. Khazan was uninstalled and explicitly excluded from this acceptance round by the user on 2026-10-04.

## Limits and retained failure evidence

- Results cover these scenes and settings, not all gameplay or NVIDIA Cubin/DLSS/frame-generation paths.
- The older intermittent Resonant initial-copy DeviceHung root cause remains unresolved. The allocator guard prevents unsafe reuse after failure; it is not proof of the original hang's cause. The current default candidate passed two successive captures and separate replays.
- Earlier PRAGMATA load and Control idle exits remain unexplained. Current successful runs do not establish their causes.
- TLOU2's first attempt failed with explicit disk-full write errors and produced an invalid RDC. The same-process retry passed after old temporary captures were preserved on another drive. The failed file is excluded from success evidence. TLOU2 exited with code 1 after the requested Alt-F4; this is recorded separately from capture survival.
- Control's replay helper overlapped the GUI replay; the other fresh independent replay checks were run with GUI replay closed or unloaded.

## Diagnostic switches

`RDOC_CAPTURE_ORDER_QUEUES=0` disables queue ordering for comparisons. `RDOC_CAPTURE_INIT_BATCH_FLUSH=0` disables capture batch flushing; value `1` additionally enables per-resource diagnostics. `RDOC_CAPTURE_DRED=1` enables capture DRED. `RDOC_CAPTURE_UAV_SHADER` plus `RDOC_CAPTURE_UAV_OUTPUT` enables the narrowly scoped compute-UAV reference diagnostic. Leave these unset for the validated default workflow.
