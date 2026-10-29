---
name: d2r-vr-settings-ui
description: >-
  Add, modify, or resolve merge conflicts for VR Settings keys, UI rows, value labels,
  help cards, INI persistence, and settings tests. Use when editing steamvr/vr_settings*
  or vr/settings_menu*.
---

# VR Settings Architecture & Modification Checklist

Not every setting touches the same files. Some settings are standard tab rows, some are paired steppers sharing a row, some are header/control boxes, some are set from the hand wheel (e.g., `LeftHandStrikes`), and some exist only in `d2r_pcvr.ini` (e.g., `DesktopOneEye`). Always inspect the current definitions in [`steamvr/vr_settings.h`](../../../steamvr/vr_settings.h) first rather than relying on memorized enum counts, row indices, or page lengths.

---

## 1. File Responsibilities & Relationships

| File | Responsibility |
| :--- | :--- |
| [`steamvr/vr_settings.h`](../../../steamvr/vr_settings.h) | Core schema and state model: `enum class Page`, `enum Key`, `ChangeMask` (`ChangeMask::Bits` capacity asserted via `static_assert(Count <= ChangeMask::Bits)`), `Definitions` array (label, detail, INI section/key, default, min, max, step), `Normalize()`, `RowEnabled()`, `RowShown()`, `PairOf()`, `RowLabel()`, `RowDetail()`, `CatalogCount()`, `CatalogKey()`, `HitTest()`, and `Model::Click()`. |
| [`steamvr/vr_settings_ui.cpp`](../../../steamvr/vr_settings_ui.cpp) & [`steamvr/vr_settings_ui.h`](../../../steamvr/vr_settings_ui.h) | Renders the settings surface (`Width` x `Height` in `Paint()`). `ValueLabel(Key, float, ...)` formats the displayed text inside each row's value box or paired stepper box. |
| [`steamvr/vr_settings_help.h`](../../../steamvr/vr_settings_help.h) | Contextual help card shown at the bottom of the panel: `HelpText(int id)`, `HelpChoices(int id)`, `HelpChoice(int id, float value)`, and `HelpAt(Page, float u, float v, ...)`. |
| [`vr/settings_menu.cpp`](../../../vr/settings_menu.cpp) & [`vr/settings_menu.h`](../../../vr/settings_menu.h) | Win32 INI persistence (`Load`, `Save`, `Submit`, `Poll`). Handles atomic temp-file writes, `.before-vr-menu` first-edit backup, conflict detection, and legacy INI key migrations. |
| [`d2r_pcvr.ini`](../../../d2r_pcvr.ini) | Shipped default configuration file with user-facing comments for INI sections and keys. |
| [`steamvr/transport.cpp`](../../../steamvr/transport.cpp) | Reads `vrsettings::Read()` snapshot on the update/render path and dispatches changed values to runtime subsystems. |
| [`steamvr/vr_settings_test.cpp`](../../../steamvr/vr_settings_test.cpp) | Unit tests for defaults, `Normalize()`, tab/row catalogs, row indices, `RowShown()`, `HitTest()`, `Model` draft edits, and `ChangeMask` isolation. |
| [`steamvr/vr_settings_store_test.cpp`](../../../steamvr/vr_settings_store_test.cpp) | Isolated temp-INI persistence tests for `vrsettings::Poll()` and `vrsettings::Submit()` round-trips, conflict detection, and legacy migrations. |
| [`steamvr/vr_settings_preview.cpp`](../../../steamvr/vr_settings_preview.cpp) | Off-screen GDI bitmap renderer and layout/pixel assertions (`--help-check`, `--worker-check`, `--performance-check`, `--release-notes-check`). |

---

## 2. Step-by-Step Checklist for Adding or Modifying a Setting

### Step 1: Schema & Normalization ([`steamvr/vr_settings.h`](../../../steamvr/vr_settings.h))
1. **`enum Key`:** Add the enumerator before `Count`. Confirm `static_assert(Count <= ChangeMask::Bits)` holds in [`steamvr/vr_settings.h`](../../../steamvr/vr_settings.h) (and update `ChangeMask` word count if `Count` ever exceeds `ChangeMask::Bits`).
2. **`Definitions`:** Add the matching `Definition{label, detail, section, key, defaultValue, min, max, step}` at the **exact same index** as in `enum Key`.
3. **`Normalize(Key, float)`:**
   - Standard `0`/`1` toggles (`step == 1`) and clamped ranges use the default clamp/round at the end of `Normalize`.
   - Discrete multi-choice enums (`min..max` integers) or custom snappers (e.g., `EnemySight`, `RunSpeed`, `HeartbeatStart`) must be added to the explicit validation branches in `Normalize()` so out-of-range or fractional INI values fall back safely.
4. **`Model::Click(Hit)`:**
   - Binary `max == 1` toggles flip automatically (`draft[key] = draft[key] > .5f ? 0.f : 1.f`).
   - Check the current branches in `Model::Click()` to see whether a multi-choice setting should clamp at its ends or wrap around, and add the key to the appropriate branch.

### Step 2: Page Placement, Pairing & Visibility ([`steamvr/vr_settings.h`](../../../steamvr/vr_settings.h))
Skip this step if the setting is INI-only or controlled solely outside the settings panel (check existing keys in [`steamvr/vr_settings.h`](../../../steamvr/vr_settings.h) to confirm).
1. **Catalog Registration:**
   - Inspect the target page's current array in `CatalogKey(Page, int)` and its return value in `CatalogCount(Page)`.
   - Add the `Key` to the page's array in `CatalogKey(Page, int)` and update `CatalogCount(Page)` to match the exact array length.
   - **Layout invariant:** Check `MaxRows` in [`steamvr/vr_settings.h`](../../../steamvr/vr_settings.h). The number of simultaneously visible rows on any page (`Rows(page, values, context).count`) must never exceed `MaxRows`. When a page's catalog has more entries than `MaxRows`, excess entries must either be mutually exclusive in `RowShown()` (such as `Snap` vs. `TurnSpeed`) or paired onto a single row via `PairOf()`.
2. **Paired Rows (if sharing a row with another `Key`):**
   - Update `PairOf(Key)` to map the primary (left) key to the secondary (right) key. Only the primary key goes into `CatalogKey()`.
   - Update `RowLabel(Key, const Context&)` and `RowDetail(Key)` if the combined row uses a shared title/subtitle.
3. **Conditional Visibility or Greying Out:**
   - `RowShown(Key, const Values&, const Context&)`: Return `false` when a row should be hidden (e.g., dependent on a parent toggle or character class). Place conditional rows below unconditional ones where possible so toggling a setting does not shift rows above the user's laser pointer.
   - `RowEnabled(const Values&, Key)`: Return `false` if the row stays visible but greyed out when inactive.

### Step 3: Value Formatting ([`steamvr/vr_settings_ui.cpp`](../../../steamvr/vr_settings_ui.cpp))
- Inspect `ValueLabel(Key key, float value, ...)` in [`steamvr/vr_settings_ui.cpp`](../../../steamvr/vr_settings_ui.cpp):
  - Standard binary toggles (`Definitions[key].max == 1`) fall through to `"On"` / `"Off"`.
  - If the setting uses custom toggle names (e.g., `"Enabled"` / `"Disabled"` or `"Enabled"` / `"Legacy"`), percentages, units, or named modes, add an `else if (key == ...)` branch before the `Definitions[key].max == 1` fallback.

### Step 4: Help Card Text ([`steamvr/vr_settings_help.h`](../../../steamvr/vr_settings_help.h))
- Add a `case <Key>:` in `HelpText(int id)` with a concise explanation of the setting and its default.
- If the setting has discrete named choices explained on the help card:
  - Add the choice count in `HelpChoices(int id)`.
  - Add the `ChoiceHelp{name, text}` mapping in `HelpChoice(int id, float value)`.
- If the control is a custom box outside the standard row list, register its hit rect in `HelpAt(...)`.

### Step 5: INI Persistence & Runtime Wiring
- **INI Load/Save ([`vr/settings_menu.cpp`](../../../vr/settings_menu.cpp)):** Standard keys in `Definitions` load and save automatically via `Load()` and `Save()`. Only edit `vr/settings_menu.cpp` if migrating a legacy INI key or writing coupled keys (inspect `Load()` and `Save()` in `vr/settings_menu.cpp` for existing coupled-key patterns).
- **Default INI ([`d2r_pcvr.ini`](../../../d2r_pcvr.ini)):** Document the new key and default under its `[section]` when shipping a user-facing setting.
- **Runtime Consumer ([`steamvr/transport.cpp`](../../../steamvr/transport.cpp) / `vr/`):** Read the normalized value from the settings snapshot and forward it to the target subsystem.

---

## 3. Updating Tests & Resolving Merge Conflicts

When adding a row or resolving merge conflicts in `steamvr/vr_settings*`:
1. **Verify Current Catalog Counts Against Source:** Do not rely on old row counts from prior tasks or branches. Inspect `CatalogKey(Page, row)` and `CatalogCount(Page)` in [`steamvr/vr_settings.h`](../../../steamvr/vr_settings.h) to verify they match and that `Rows(page, ...).count <= MaxRows` holds for all states.
2. **Inspect Shifted Row Indices in [`steamvr/vr_settings_test.cpp`](../../../steamvr/vr_settings_test.cpp):**
   - Inserting, removing, or pairing a row shifts subsequent indices in `RowKey(Page::..., index)`, `Decrease(index)`, `Increase(index)`, and `ValueBox(index)`. Search [`steamvr/vr_settings_test.cpp`](../../../steamvr/vr_settings_test.cpp) for all references to that `Page` before assuming which row indices are asserted.
   - Verify footer clearance assertions (`ValueBox(row).y + ValueBox(row).h <= ...`) still pass for the bottom-most visible row on the page.
3. **Add Focused Unit Coverage:**
   - In [`steamvr/vr_settings_test.cpp`](../../../steamvr/vr_settings_test.cpp): Verify default value, `RowKey` index, `Normalize` bounds, `HelpText`/`HelpChoice` non-empty strings, stepper/value clicks, and `Apply` `ChangeMask` isolation (`request.changed == (ChangeMask{1} << Key)`).
   - In [`steamvr/vr_settings_store_test.cpp`](../../../steamvr/vr_settings_store_test.cpp): Add a round-trip persistence check if custom load/save or legacy migration logic was touched.

---

## 4. Narrowest Relevant Validation Commands

Check [`steamvr/CMakeLists.txt`](../../../steamvr/CMakeLists.txt) for the current settings-related targets and `add_test` registrations, then build and run only those targets:

```powershell
.\deps\cmake-4.4.4-windows-x86_64\bin\cmake.exe --build build-frame/windows-native --parallel 4 --target vr-settings-test vr-settings-store-test vr-settings-preview controller-labels-test
.\deps\cmake-4.4.4-windows-x86_64\bin\ctest.exe --test-dir build-frame/windows-native -R "^(vr-settings|settings-release-notes|controller-labels)" --output-on-failure
```
*(If your local `deps/` CMake directory version differs, substitute the `cmake.exe` / `ctest.exe` path provisioned under `deps/`.)*
