# SampQOL 0.3.0 offset map

What `src/common/qol_map.h` reads in `mpcqol.so`, and why each 0.3.0 value is the 0.2.0 one.
Binaries: 0.2.0 sha256 `fc9285ac…`, 0.3.0 sha256 `facea6b4…` (both aarch64, stripped, symbols from
`nm -D`). Disassembly: `llvm-objdump -d`. A build is told apart by the offset of its exported
`overlay_update` (0.2.0 `0x61be4`, 0.3.0 `0x6e220`); any other build gets no map and every hook
stays off.

Functions are not mapped: the kit already reaches them through `dlsym(RTLD_NEXT, name)`. The old
fixed-offset guards on them became `qol_owns()`: the symbol must resolve inside `mpcqol.so`.

Confidence: **high** = same instruction at the same place in both builds, or the only
counterpart there is; **medium** = same pattern, but semantics carried over by inference.

## Pad mixer state (data)

| Name | 0.2.0 | 0.3.0 | Evidence | Confidence |
|---|---|---|---|---|
| `padmix_open` | `0x1632688` | `0x16d8398` | Exported getter `padmix_open`: `adrp x8,…; add x8,#0x688; ldar w0,[x8]` (0.2.0 `0xa32d0`) vs `add x8,#0x398; ldar` (0.3.0 `0xb93dc`). | high |
| `insert_editor` | `0x16328dc` | `0x16d8604` | `padmix_show_master` clears it twice, right after `padmix_open`: `add #0x8dc; stlr wzr` at `0xa3374`/`0xa34f4`, `add #0x604; stlr wzr` at `0xb9480`/`0xb9600`. Its neighbours line up one for one (8d8→600, 904→5f8, 908→62c, 910→638, 950→6f8, a00→7b8). | high |
| `insert_uid` | `0x16328f4` | `0x16d861c` | `padmix_note` passes it as the 5th argument of the same call: `ldr w1,#0x8e0 / w2,#0x8e4 / x4,#0x8e8 / w5,#0x8f0 / w6,#0x8f4; mov w0,#1; bl` at `0xa3f70`, and `#0x608 / #0x60c / #0x610 / #0x618 / #0x61c` at `0xba2dc`. Stored by the same block in the editor open: `str w21,[x8,#0x8f4]` `0xaecec` vs `#0x61c` `0xc6280`. | high |
| `insert_row` | `0x1632908` | `0x16d862c` | `padmix_show_master`: `stlr w19,[0x…904]` then `stlr wzr,[0x…908]` (`0xa3398`, `0xa33a4`) vs `stlr w19,[0x…5f8]`, `stlr wzr,[0x…62c]` (`0xb94a4`, `0xb94b0`). | high |
| `set_pending` | `0x1632960` | `0x16d8704` | `padmix_set`'s one-at-a-time flag: `add #0x960; ldaxr/stlxr` spin at `0xa7860`, `stlr wzr` at `0xa7900`, vs `add #0x704` at `0xbe5d4` / `0xbe670`. | high |
| `editor` | `0x16333a0` | `0x16d9160` | Editor open: `bl eq_spectrum_clear; …; stlr x22,[0x…8f8]; stlr x22,[0x…3a0]` (`0xaee64`) vs `stlr x22,[0x…620]; stlr x22,[0x…160]` (`0xc63f8`). Same 1 read + 3 writes in both. The function that stores it (`0xaebf4` / `0xc6188`) is instruction-identical up to the store, so the object behind it (insert at `+0xd0`, LADSPA descriptor `+0x128`, handle `+0x130`) is the same MPC object. | high (pointer), medium (fields) |

## Mixer (data)

| Name | 0.2.0 | 0.3.0 | Evidence | Confidence |
|---|---|---|---|---|
| `mix_snapshot` | `0x1957ddc` | `0x19ff710` | Only `mixview_frame` uses it, 20 times in both, at matching places (`add #0xddc` at `0xdf25c` … `0xe2178`; `add #0x710` at `0xfa504` … `0xfd4d0`); next fields match by use count (de0→714, de4→718, de8→71c). | high |
| `mix_selected` | `0x1957948` | `0x19ff248` | Same users and counts (`mixview_note` 6, `mixview_cc` 8, `mixview_frame` 1); `add x19,#0x948` and `#0x248` at aligned places in `mixview_frame`. | high |
| `mix_editing` | `0x1957930` | `0x19ff230` | Same users and counts (note 1, cc 2, frame 1); `add x20,#0x930` vs `#0x230` at aligned places. | high |
| `strip_bytes` | 116 | **120** | Every strip stride in `mixview_frame` is `#0x74` in 0.2.0 and `#0x78` in 0.3.0; the field at strip `+0x70` grew from `w` to `x`. Strips still start at snapshot `+12`; the copy into the strip writes the same offsets below `+0x70` (`stp q0,q1,[x10,#0x40]`, `[x10,#0x20]`), so every field the console reads (4, 40–92) is unchanged. | high (stride), medium (fields) |

## Overlay layers

`overlay_update(mode, …)` indexes a 56-entry layer table (0.2.0 `0x112af0`, 0.3.0 `0x13ab90`). Matching
the layer objects (0.3.0 address = 0.2.0 + `0x29030`) shows one layer moved below 26, so 0.2.0
modes 25–55 are 24–54 on 0.3.0. The same layer objects means the same sizes.

| Layer | 0.2.0 | 0.3.0 |
|---|---|---|
| header 320x31 | 26 | 25 |
| list 320x160 | 27 | 26 |
| soft keys 320x40 | 28 | 27 |
| mixer console | 33 | 32 (`mov w0,#0x21` → `#0x20` at the aligned call in `mixview_frame`) |

Checked at run time in an arm64 container with both real `mpcqol.so` files: `hs-chrome.so` paints
its red header bar on mode 26 under 0.2.0 and mode 25 under 0.3.0.

## Return sites (code, inside the pad mixer's draw function)

Pairs found by aligning the function (`padmix_init` region, 0.2.0 `0xa9000…`, 0.3.0 `0xbf000…`),
then checked by buffer register, mode and the instructions after the call.

| Kit use | 0.2.0 | 0.3.0 | Match |
|---|---|---|---|
| header (B2 page label) | `0xabc60` | `0xc2ec4` | aligned; `mov x1,x19` header buffer, `ldr w19,[sp,…]` both |
| header, global buffer | `0xac684` | `0xc4d98` | only header call drawing a global 320x31 buffer after `text_width`/`text_draw` |
| header, ducker page | `0xad644` | `0xc4890` | aligned (16-instruction run) |
| list, stock name | `0xad9e8` | `0xc4b98` | reached by `cbz` from the block that sets `mov w0,#list; mov x1,list` and `ldr w21,[sp,#0x150]` |
| list, name + rack | `0xab89c` | `0xc28b0` | `mov w2,#1; mov w28/w19,#1`; tail `mov w8,#1; … ldr w21,[sp,#0x150]` |
| list, insert body | `0xac2f0` | `0xc3558` | `ldr w21,[sp,…]; mov w0,#list; mov x1,list; mov w2,#1` |
| list, selector | `0xad460` | `0xc43d8` | `b back; mov w0,#list; mov x1,list` |
| list, effect editor | `0xada58` | `0xc4c80` | tail `mov w8,#1; str wzr,[sp,…]; stur w8,[x29,#-0x8c]`; entered by `cbz` after the rect call |
| soft keys, selector | `0xad4ac` | `0xc4434` | aligned |
| soft keys, knobs | `0xad698` | `0xc48e8` | aligned |
| rect list 0,40,320,160, effect | `0xaca6c` | `0xc39fc` | tail `str wzr,[sp,…]; b` |
| rect list 0,40,320,160, name | `0xabb98` | `0xc2df0` | aligned |
| rect soft keys 0,200,320,40 | `0xad93c` | `0xc4b88` | the only rect call with that geometry in each |

## Unchanged in 0.3.0

- `eq_draw` is the same code (1874 of 1876 instructions equal; the two differences are string
  addresses), so `hs_eq.c`'s graph geometry holds.
- `fxlist_get` is still called once with capacity 40 and 36-byte rows (`mov w8,#0x28; sub w2,w8,w22`
  at `0xb1934` / `0xc9a24`). Stock rows: the hidden-effects loop runs to 32 in 0.2.0
  (`cmp w22,#0x20`) and 34 in 0.3.0 (`cmp w22,#0x22`): AIR Lo-Fi and AIR Pitch Shifter.
- `context_locate_plugin_manager`, `padmix_force`, `overlay_update`, `overlay_update_rect`:
  instruction-identical.

## Not in this repo

The card repo's `graphic_mixer.c` also writes to `base+0x1480ff0`. The public kit has no such
write, so nothing here maps it.
