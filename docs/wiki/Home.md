# LifeGameDxlibVersion3 開発資料

このディレクトリは、後から開発を再開するときに仕様・設計判断・性能改善の経緯を復元するためのWiki草案です。

**資料用PRからmainへマージすることは前提にしません。** 現行仕様の正本は常にmainのソースコードです。

## ページ

- [Current-Spec.md](Current-Spec.md): 現在の盤面・Camera・UI・編集・保存仕様
- [Board-Architecture.md](Board-Architecture.md): Sparse Chunkとstep()の構造
- [Save-Format-and-Coordinates.md](Save-Format-and-Coordinates.md): .ary3、負座標、整合性ガード
- [Performance-History.md](Performance-History.md): 約34 gen/sから約60 gen/sまでの最適化履歴
- [PR-History.md](PR-History.md): PR #1以降の索引
- [Development-Handoff.md](Development-Handoff.md): 開発再開時の確認順・テスト手順
- [Roadmap.md](Roadmap.md): 現在の次段階

## 基本方針

Version3はDxLib + C++によるConway's Game of Life (B3/S23)で、盤面全体の固定幅・固定高を持たない仮想無限平面を採用する。

64bit符号付きセル座標を64x64 Chunkへ分割し、生存セルが存在する周辺だけを保持する。Version2由来のPatternLibrary/RLE/UI等は、この無限平面モデルへ段階的に移植している。

性能最適化では「理屈上速そう」だけで採用せず、巨大セーブデータによる実測とF9 correctness self-testを重視する。
