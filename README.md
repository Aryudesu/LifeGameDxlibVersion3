# LifeGameDxlibVersion3

DxLib + C++ で実装する Conway's Game of Life の Version 3 です。

Version 2 の有限 BitBoard 版を土台にしつつ、Version 3 では **仮想無限平面** を前提とした Sparse Chunk 方式を実験・発展させます。

## 方針

- 盤面全体の幅・高さを持たない
- 負の座標を含む 64bit 整数座標を扱う
- 生存セルが存在する周辺だけを Chunk として保持する
- Camera は無限平面を自由に移動する
- 最初は分かりやすい Sparse Chunk 実装を優先し、後から BitBoard 化・アクティブ Chunk 最適化・HashLife 等を検討する

## 現在の操作

- `Enter`: pause / resume
- `Space`: pause 中に1世代進める
- `PageUp / PageDown`: シミュレーション速度変更
- 矢印キー: Camera移動
- マウスホイール: カーソル基準ズーム
- 中ボタンドラッグ: Camera移動
- pause 中 + 左ドラッグ: 生存セルを連続配置
- pause 中 + 右ドラッグ: セルを連続削除
- `Delete`: 全消去してpause

## 最初のマイルストーン

1. Sparse Chunk 盤面
2. Conway B3/S23 の世代更新
3. 無限 Camera / 描画
4. Glider が何千世代でも盤面端に阻まれず移動できることを確認
5. Version 2 の PatternLibrary / RLE / UI / 保存機能を段階的に移植
