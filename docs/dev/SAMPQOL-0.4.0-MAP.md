# SampQOL 0.4.0 offset map

What `src/common/qol_map.h` reads in `mpcqol.so` 0.4.0, and where each value comes from.
Binary: the `mpcqol.so` of the public 0.4.0 zip (GitHub snarebes/SampQOL-OS, release v0.4.0),
sha256 `d16501b63a828b1104fbbdad2b3f5230e325bda955dfd3454b8b4b73fff7294e`. The exported
`overlay_update` is at `0x803c4`.

## Method

This map does not come from aligning disassembly. SampQOL builds `mpcqol.so` by stripping
`mpcqol.debug.so`, so the code in both is the same. A rebuild of the 0.4.0 release commit gives
the exact sha256 above. The values come from that debug build:

- data: the address of the static variable in the SampQOL source (DWARF + symbol table);
- layer ids: the value of the `OV_*` enumerator (DWARF);
- `strip_bytes`: `sizeof(struct mixstrip)` (DWARF);
- return sites: each `bl overlay_update@plt` / `bl overlay_update_rect@plt` inlined into
  `ui_thread()` in `padmix.c`, found by its source line (call address + 4).

The same method on a rebuild of the 0.3.0 release (sha256 `facea6b4…`, exact) gives the 0.3.0
entry of `qol_map.h` value for value. So each field below is the same source item as in 0.3.0.

Confidence: **high** for every value (same source item, read from the debug build). The kit has
not been run on 0.4.0 hardware with this map.

## Pad mixer state (data, `padmix.c`)

| Name | 0.4.0 | Source | Same as 0.3.0 |
|---|---|---|---|
| `padmix_open` | `0x18e2d40` | `g_open` | declaration unchanged |
| `insert_editor` | `0x18e2d78` | `g_page` | declaration unchanged |
| `insert_uid` | `0x18e2fe4` | `g_pg_uid` | declaration unchanged |
| `insert_row` | `0x18e2ff4` | `g_sel` | declaration unchanged, `NSLOTS` still 4 |
| `set_pending` | `0x18e7ce4` | `g_set_posted` | declaration unchanged |
| `editor` | `0x18e8798` | `g_pg_fx` | declaration unchanged |

## Mixer (data, `mixview.c`)

| Name | 0.4.0 | Source |
|---|---|---|
| `mix_snapshot` | `0x1d88fd0` | `g_snap` (`struct mixsnap`: `view, n, cur`, strips from `+12`) |
| `mix_selected` | `0x1d88b08` | `g_sel` |
| `mix_editing` | `0x1d88af0` | `g_edit` |
| `strip_bytes` | 120 | `sizeof(struct mixstrip)`, unchanged |

`struct mixstrip` is the same as in 0.3.0: `num +4, colour +40, vol +44, gain +48, pan +52,
peak +56/+60, hold +64/+68, mute +72, solo +76, silent +80, can_mute +84, can_solo +88, out +92`.

## Overlay layers

| Layer | Enumerator | 0.4.0 |
|---|---|---|
| header 320x31 | `OV_PMT` | 25 |
| list 320x160 | `OV_PML` | 26 |
| soft keys 320x40 | `OV_PMK` | 27 |
| mixer console | `OV_MXM` | 32 |

0.4.0 adds 6 layers, all after the last 0.3.0 layer, so the ids do not change. The sizes
(`PMT/PML/PMK/MXM_*`) are the same.

## Return sites (code, `padmix.c` `ui_thread()`)

Same field order and the same source call as the 0.3.0 entry. "redraw" = the `show()` inside
`redraw()`, "rect" = the `overlay_update_rect` of `redraw()`.

| Kit field | 0.4.0 | `padmix.c` line | Call |
|---|---|---|---|
| `at_header` | `0xe0ed8` | 5385 | `render_ins_top(); show(OV_PMT, …)` |
| `at_header_list` | `0xe2df4` | 5499 | `render_top(&v, sel, sh); show(OV_PMT, …)` |
| `at_header_ducker` | `0xe291c` | 5456 | `render_page_top(&pv); show(OV_PMT, …)` |
| `at_list_name` | `0xe2c44` | 5440 | filter page: `redraw(OV_PML, …)`, redraw |
| `at_list_name_rack` | `0xe0878` | 5434 | Kill EQ page: `show(OV_PML, …)` |
| `at_list_body` | `0xe15cc` | 5493 | `render_list(&v, sel); show(OV_PML, …)` |
| `at_list_selector` | `0xe2450` | 5390 | insert list: `show(OV_PML, …)` |
| `at_list_effect` | `0xe2d1c` | 5449 | effect page: `redraw(OV_PML, …)`, redraw |
| `at_soft_selector` | `0xe24b8` | 5394 | insert list: `show(OV_PMK, …)` |
| `at_soft_knobs` | `0xe2978` | 5462 | Kill EQ page: `show(OV_PMK, …)` |
| `rect_list_effect` | `0xe1a60` | 5449 | effect page: `redraw(OV_PML, …)`, rect |
| `rect_list_name` | `0xe0e08` | 5440 | filter page: `redraw(OV_PML, …)`, rect |
| `rect_soft_knobs` | `0xe2c2c` | 5465 | `redraw(OV_PMK, …)`, rect |

Each call is the only one with that line and path. `ui_thread()` has 2 new calls in 0.4.0, both
for the new layer `OV_PF1` (Input Pad FX "< Back" label). The kit does not map them.

## Unchanged in 0.4.0

The prototypes of `fxlist_get`, `fxmap_get_row`, `padmix_set/note/cc/force/open`,
`overlay_update`, `overlay_update_rect`, `mixview_frame`, `eq_draw`, `eq_spectrum_draw` and
`context_locate_plugin_manager` are the same as in 0.3.0. `struct fxrow` is still 36 bytes and
`FXLIST_MAX` is still 40. `eq_draw`, `eq_spectrum_draw`, `fxlist_get`, `fxmap_get_row`,
`padmix_force`, `padmix_set`, `padmix_cc` and `context_locate_plugin_manager` have the same
size as in 0.3.0.

## Changed in 0.4.0 (behaviour the kit can see)

- **Input FX.** SAMPLE RECORD > Input FX opens the same Pad Mix page: `padmix_open` is 1 there
  too, and the list, header and soft-key sites above draw it.
- **One row less on the Input list.** The insert list of the Input level has an own top row
  "Pad FX" (in no group). `fxlist_get` is then called with `max` one lower (39 minus the hidden
  rows), so the HS rows have one place less there.
- **`mixview_frame` can return early.** On a change of view (B1 / B2) it keeps the old page up
  to 6 frames (~300 ms), until the first read of the new view lands.
- **`overlay_update` waits.** It can hold a hide or a first show for the next stock frame
  (max ~100-300 ms) on layers with a live function, the Pad Mix layers included. Same
  prototype; the kit's own calls go through it as before.
- **Stems.** 0.4.0 has its own Stems Separation on SHIFT + B3 (see PR #26).

## 0.4.1

0.4.1 changes none of the files behind this map (`padmix.c`, `mixview.c`, `mixmeter.*`,
`overlay.*`, `eq.c`, `equi.c`). Only the addresses move. Its entry follows from the same method on
the 0.4.1 release build, once that build is final.
