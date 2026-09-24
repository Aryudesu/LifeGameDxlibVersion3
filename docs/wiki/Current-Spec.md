# 現行仕様

## 盤面

- セル座標: signed 64bit (`InfiniteLifeBoard::Coord = int64_t`)
- Chunk: 64 x 64
- Chunk本体は `unordered_map<ChunkCoord, Chunk>` に疎に保持
- 各Chunkは64本のuint64 row bitsetを持つ
- `nonEmptyRows`, `westEdgeRows`, `eastEdgeRows` をメタデータとして保持
- Life rule: Conway B3/S23

## Camera / 描画

- Camera自体も64bit座標
- 矢印キー: Camera移動
- ホイール: マウス位置基準zoom
- 中ボタンドラッグ: pan
- zoom: 1..32 cell pixels
- 描画は `forEachAliveCellInRect()` により可視Chunkだけを走査する
- G: grid ON/OFF

## Simulation

- Enter: pause/resume
- Space: pause中に1 generation step
- PageUp/PageDown: 1,5,10,30,60,120,300,600 gen/s
- UIにはGeneration/FPS/Speed/Alive/Chunks/Camera/Zoom/Grid/Undo/Redo等を表示
- PerformanceLoggerで実測gen/s等をCSV記録

## 編集

pause中のみ編集する。

- 左ドラッグ: cell追加
- 右ドラッグ: cell削除
- Cell strokeはマウス移動間をline補間し、セル飛びを防止
- Line / Rectangle / Circle tool
- LineはShiftで45度snap、RectangleはShiftでsquare
- Undo: Ctrl+Z
- Redo: Ctrl+Y / Ctrl+Shift+Z
- Delete: 全消去
- Pattern placementはghost previewを表示
- Pattern rotation: Q/EおよびUI

編集履歴はセル単位のbefore/after差分をCommandとして保持し、最大256 command。

## Pattern

`PatternLibrary` のcategory:
- Cell
- StillLife
- Oscillator
- Methuselah
- Spaceship
- Gun

RLE parserも移植済み。

## Selection

PR #37でmainへ導入済み。

- V: 選択モード ON/OFF
- LMB drag: 矩形選択
- M: 通常矩形 / 生存セルのみのmask切替
- 通常選択は青枠、live-cell maskは橙枠 + 対象セルを橙表示
- 通常矩形はboundsのみ保持し、live-cell maskは疎なcell listとして保持

PR #39でCopy/Cut/Paste + ghost previewを開発中。
