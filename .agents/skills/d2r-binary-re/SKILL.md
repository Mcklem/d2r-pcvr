---
name: d2r-binary-re
description: >-
  Reverse engineer Diablo II: Resurrected native functions, inspect decrypted code,
  verify RVAs and byte signatures, and validate hook ABI and thread safety. Use when
  adding or debugging MinHook detours, signature scans, struct offsets, pointer chains,
  or native game calls.
---

# D2R Binary Reverse Engineering & Signature Verification

## 1. Critical Rule: Do Not Disassemble On-Disk `D2R.exe` Directly

The on-disk `D2R.exe` binary is packed/encrypted. Its `.text` section on disk contains encrypted bytes until unpacked in memory at runtime (and some pages decrypt lazily when first executed, as noted in [`cleanroom/sigscan/sigscan.h`](../../../cleanroom/sigscan/sigscan.h)).

- **Never** run `llvm-objdump`, IDA, or byte-offset reads directly against the on-disk `D2R.exe` file to inspect instructions—doing so returns encrypted bytes and invalid instructions (`<unknown>`).
- **Always** inspect decrypted code from either:
  1. Existing tracked signature/layout headers in the repository,
  2. Read-only live process memory (`ReadProcessMemory`), or
  3. An optional local decrypted memory snapshot (git-ignored directories such as `dump/`, `dump_ingame/`, `dump_loader/`, or `build-frame/*.bin`, if present on the machine).

---

## 2. Existing Tracked Sources of Evidence (Consult First)

Before searching memory or writing new hooks, check the repository's tracked inventory:

1. **Relocatable Signature Table & Scanner:**
   - [`cleanroom/sigscan/game_sigs.h`](../../../cleanroom/sigscan/game_sigs.h), [`cleanroom/sigscan/sigscan.h`](../../../cleanroom/sigscan/sigscan.h), and [`cleanroom/sigscan/sigscan.cpp`](../../../cleanroom/sigscan/sigscan.cpp) (`d2rsig::Resolve`, `d2rsig::Rva`, `d2rsig::Addr`, `d2rsig::Check`, `d2rsig::Hook`, `d2rsig::Patch`).
   - Baseline RVAs in `game_sigs.h` correspond to the reference build documented in that header (image base `0x140000000`). Across other game builds, `sigscan.cpp` locates moved functions by pattern (`??` wildcards for RIP-relative displacements and call targets) and writes diagnostic status to `d2r_vr_game_code.txt`.
2. **Camera & UI Layout Constants:**
   - [`cleanroom/camera/game_layout.h`](../../../cleanroom/camera/game_layout.h) documents verified camera struct offsets, vtable slots, and hook entry bytes.
3. **Subsystem Signature Headers (`vr/*_signatures.h`):**
   - [`vr/billboard_signatures.h`](../../../vr/billboard_signatures.h)
   - [`vr/bow_signatures.h`](../../../vr/bow_signatures.h)
   - [`vr/breakables_signatures.h`](../../../vr/breakables_signatures.h)
   - [`vr/companion_ai_signatures.h`](../../../vr/companion_ai_signatures.h)
   - [`vr/holy_bolt_signatures.h`](../../../vr/holy_bolt_signatures.h)
   - [`vr/melee_weapon_signatures.h`](../../../vr/melee_weapon_signatures.h)
   - [`vr/native_ui_signatures.h`](../../../vr/native_ui_signatures.h)
   - [`vr/offline_difficulty_signatures.h`](../../../vr/offline_difficulty_signatures.h)
   - [`vr/quick_items_signatures.h`](../../../vr/quick_items_signatures.h)
   - [`vr/spell_aim_signatures.h`](../../../vr/spell_aim_signatures.h)
   - [`vr/spell_signatures.h`](../../../vr/spell_signatures.h)
4. **In-Game Diagnostic Modules (`vr/`):**
   - [`vr/probe.cpp`](../../../vr/probe.cpp): One-shot read-only heap scan (`F10`) for Granny skeletons, bone parent hierarchies, and pose matrix layouts; writes `d2r_vr_probe.txt`.
   - [`vr/memdiff.cpp`](../../../vr/memdiff.cpp): Read-only differential scanner (`Ctrl+F9` / `Ctrl+Shift+F9`) of writable non-executable image sections (`.data`) to isolate global flags; writes `d2r_vr_memdiff.txt`.
   - [`vr/writewatch.cpp`](../../../vr/writewatch.cpp): Short-duration hardware data breakpoint (`Dr0`/`Dr1`) recorder that unwinds the call stack (`Rip` + callers) of threads writing to a watched address.
   - [`vr/uitrace.cpp`](../../../vr/uitrace.cpp): Single-frame D3D12/ReShade command-list and render-target tracer (`Ctrl+F10`); writes `d2r_vr_uitrace.txt`.

---

## 3. Auditing Signatures & Disassembling Decrypted Snapshots

### A. Read-Only Live Process Audit (Tracked Script)
When the user has the game running and wants to verify whether baseline RVAs match the live process without injecting or modifying memory:
```powershell
.\deps\python\python.exe steamvr/audit_live_game.py --pid <PID> --game "<GAME_DIR>" --output build-frame/live-audit.json
```
*(Check [`build-frame/AGENT_LOCAL_STATE.md`](../../../build-frame/AGENT_LOCAL_STATE.md) if present for the local `<GAME_DIR>` path.)*
[`steamvr/audit_live_game.py`](../../../steamvr/audit_live_game.py) opens the process with `0x410` (`PROCESS_QUERY_INFORMATION | PROCESS_VM_READ`) only and checks all `vr/*_signatures.h` and `cleanroom/sigscan/game_sigs.h` sites.

### B. Offline Decrypted Snapshot Audits (Optional Local Snapshots)
When a git-ignored local decrypted code snapshot is available (by convention dumped starting at RVA `0x1000`, so `snapshot_offset = rva - 0x1000`), run the tracked audit scripts against it:
```powershell
.\deps\python\python.exe steamvr/audit_bow_signatures.py <snapshot-path>
.\deps\python\python.exe steamvr/audit_interaction_signatures.py <snapshot-path>
.\deps\python\python.exe steamvr/audit_spell_aim.py <snapshot-path>
```
To disassemble a specific RVA from an optional local decrypted PE dump using the toolchain in `deps/` (after running [`steamvr/setup_windows_tools.ps1`](../../../steamvr/setup_windows_tools.ps1)):
```powershell
.\deps\llvm-mingw-20260616-ucrt-x86_64\bin\llvm-objdump.exe -d --start-address=0x140381890 --stop-address=0x140381910 <decrypted-pe-dump>
```
Never commit or attach any snapshot or memory dump (see [`.gitignore`](../../../.gitignore) and [`CONTRIBUTING.md`](../../../CONTRIBUTING.md)).

---

## 4. Verifying ABI, Detours, Memory Access & Thread Affinity

Before calling or hooking any native function, verify all five dimensions:

1. **Calling Convention & Register Arguments (Windows x64 ABI):**
   - First 4 arguments: `rcx`, `rdx`, `r8`, `r9` (floating-point in `xmm0`–`xmm3`); 5th+ arguments on stack at `[rsp+0x20]`, `[rsp+0x28]`, etc.
   - Verify from caller disassembly what `rcx` actually is (e.g., server `D2GameStrc*`, client/server `D2UnitStrc*`, `D2DynamicPathStrc*`, `Camera*`, or global manager). Passing a `Unit*` to a function expecting a `Path*` or `Game*` will corrupt state or crash.

2. **Detour Mechanics, Complete Instruction Boundaries & Trampoline Requirements:**
   - Check which hooking path the subsystem uses and verify its exact constraints:
     - **D2RLoader inline hooks via [`cleanroom/sigscan/sigscan.cpp`](../../../cleanroom/sigscan/sigscan.cpp):** `d2rsig::Hook(rva, expected, size, detour, original)` validates expected bytes (`CheckMoved` / `CheckExpectedBytes`) before delegating to `g_ctx->InstallInlineHook(at, expected, size, detour, original)`.
     - **Direct MinHook detours via [`third_party/minhook/src/trampoline.c`](../../../third_party/minhook/src/trampoline.c):** `CreateTrampolineFunction` decodes instructions sequentially with HDE64 (`hde64_disasm`) until the copied span covers at least `sizeof(JMP_REL)` (a 5-byte `E9 rel32` jump, or a 2-byte `EB rel8` hot-patch jump if 5 bytes of filler precede the target), up to `TRAMPOLINE_MAX_SIZE` (50 bytes) and at most 8 instructions.
   - **Instruction-boundary & branch requirements:**
     - The overwritten region must end on a complete instruction boundary covering at least the 5-byte relative jump (`oldPos >= sizeof(JMP_REL)`).
     - MinHook relocates RIP-relative memory operands (`(hs.modrm & 0xC7) == 0x05`), relative `CALL` (`0xE8`), relative `JMP` (`0xEB`/`0xE9`), and `Jcc` instructions, **except**:
       - `LOOPNZ`/`LOOPZ`/`LOOP`/`JCXZ`/`JECXZ` (`0xE0`–`0xE3`) branching outside the overwritten span are rejected (`return FALSE`).
       - Internal branches targeting an offset *inside* the overwritten region prevent instruction-length expansion (`copySize != hs.len` fails if `pOldInst < jmpDest`), and any external caller or loop jumping into a mid-instruction or post-entry offset inside the overwritten `JMP_REL` bytes will crash. (See how `ReleaseCamera` in [`cleanroom/camera/game_layout.h`](../../../cleanroom/camera/game_layout.h) hooks at `+5`, past an entry `test rcx,rcx / je`, so the short branch remains outside the overwritten span.)
   - **Mandatory signature verification:** Always validate expected bytes (`d2rsig::Hook`, `d2rsig::Check`, or explicit `Sig[]` verification) before installing a detour or calling an RVA so a mismatched game build disables the feature cleanly.

3. **Read-Only Memory Inspection vs. Intentional Memory Modification ([`steamvr/memory.h`](../../../steamvr/memory.h)):**
   - **Read-only inspection (`framevr::ReadMemory`):** Never dereference untrusted or multi-level game pointers directly during probing or state reads. `framevr::ReadMemory(void* dst, const void* src, size_t size)` calls `ReadProcessMemory(GetCurrentProcess(), src, dst, size, &copied) && copied == size` so an unmapped or invalid source address returns `false` inside `ntdll` instead of raising an access violation into D2R's crash handler.
   - **Intentional runtime modification (`framevr::WriteMemory` vs. `d2rsig::Patch`):**
     - Keep read-only reconnaissance strictly separate from state mutation.
     - In [`steamvr/memory.h`](../../../steamvr/memory.h), `framevr::WriteMemory(void* dst, const void* src, size_t size)` also uses `ReadProcessMemory(GetCurrentProcess(), src, dst, size, &copied)` (reading from `src` into `dst`) because Wine/Windows current-process `ReadProcessMemory` validates both source and destination buffers locally inside `ntdll` without raising an application crash and without serializing through `wineserver` (unlike `WriteProcessMemory`). Because it does **not** alter page protections, `framevr::WriteMemory` only writes to **already-writable** in-process data buffers.
     - For executable code patches, never use `framevr::WriteMemory`; use `d2rsig::Patch(rva, expected, patch, size)` in [`cleanroom/sigscan/sigscan.cpp`](../../../cleanroom/sigscan/sigscan.cpp), which verifies the expected original bytes before calling `g_ctx->PatchBytes`.

4. **Thread Affinity & Synchronization:**
   - Identify which thread owns the target data or function:
     - **Server / Game simulation thread** (e.g., combat targeting, unit path/room updates in [`vr/melee.cpp`](../../../vr/melee.cpp) and [`vr/companion_ai.cpp`](../../../vr/companion_ai.cpp)).
     - **Client / UI / Render-tick thread** (e.g., camera follow in [`cleanroom/camera/d2rcam.cpp`](../../../cleanroom/camera/d2rcam.cpp), skeleton pose evaluation in [`vr/skeletons.cpp`](../../../vr/skeletons.cpp)).
     - **OpenXR / D3D12 submission thread** ([`steamvr/transport.cpp`](../../../steamvr/transport.cpp)).
   - Never invoke game-thread mutations directly from the OpenXR thread. Queue work to the appropriate hook tick and guard asynchronous loader callbacks across level/world transitions with [`steamvr/callback_ticket.h`](../../../steamvr/callback_ticket.h) (`framevr::CallbackTicket`).

5. **Confirmed Facts vs. Hypotheses:**
   - Do not call an unverified RVA in a periodic tick loop to "see if it works."
   - Separate confirmed observations (verified bytes, disassembled callers, logged register values) from hypotheses, and add a deterministic signature/mismatch unit test (see [`steamvr/map_sigs_test.cpp`](../../../steamvr/map_sigs_test.cpp) or [`steamvr/quick_items_test.cpp`](../../../steamvr/quick_items_test.cpp)) when locking in a signature guard.
