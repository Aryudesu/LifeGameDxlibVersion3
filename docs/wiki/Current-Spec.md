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

PR #37で矩形選択・mask基盤を開発中。mainへ未マージのため、現行main仕様にはまだ含めない。
