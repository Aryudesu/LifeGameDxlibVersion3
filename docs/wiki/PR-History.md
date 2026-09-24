# PR history

## Foundation / feature

- #1 仮想無限平面の最小prototype — merged
- #2 Version2相当の速度制御とCamera操作 — merged
- #3 PatternLibrary / RLE移植 — merged
- #4 右側tool panel / pattern list UI — merged
- #5 無限平面save/load — merged
- #6 Chunk単位step高速化 — merged
- #7 Pattern ghost preview — merged
- #8 Undo/Redo — merged
- #9 Cell drag interpolation — merged
- #10 Line/Rectangle/Circle — merged
- #11 icon toolbar — merged
- #12 Save/Load座標ずれdiagnostic — closed without merge
- #13 performance log拡張 — merged
- #14 負座標Chunk分解hardening — merged
- #15 Load integrity guard — merged
- #16 visible Chunk rendering — merged

## Performance investigation

- #17 Chunk hash experiment — closed without merge
- #18 direct step/render profiling — closed without merge
- #19 Candidate 3x3 neighborhood cache — merged
- #20 detailed step profiling — closed without merge
- #21 next map bucket reuse — closed without merge
- #22 current map node reuse — closed without merge
- #23 row materialization — merged
- #24 CSA neighbor count — merged
- #25 post-CSA profiling — closed without merge
- #26 Candidate generation experiment — closed without merge
- #27 lazy row materialization — closed without merge
- #28 full flat Candidate table — closed without merge
- #29 dense Candidate + flat index — merged
- #30 post-#29 profiling — closed without merge
- #31 next Chunk metadata experiment — closed without merge
- #32 CSA mask simplification — closed without merge
- #33 growable Candidate index — merged
- #34 Candidate reserve amount experiment — closed without merge
- #35 Candidate reserve diagnostic — closed without merge
- #36 Candidate reserve position diagnostic — closed without merge

## Current feature work

- #37 矩形選択と生存セルmask基盤 — merged
- #38 [DO NOT MERGE] Wiki草案 — 長期保存・更新用
- #39 Selection Clipboard / Copy/Cut/Paste / ghost preview — Draft / development中

性能PRは「mergeされたものだけが成功」ではない。不採用実験も、次の設計（特に#28→#29）を決める重要な資料として残す。
