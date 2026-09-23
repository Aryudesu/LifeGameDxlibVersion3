# Roadmap

## 現在

性能改善章は、#33までで巨大fixture約60 gen/sに到達し、一旦区切り。

次の大きなテーマは編集機能。

## Selection / Clipboard

PR #37から段階的に進める。

想定:
1. 矩形選択
2. mask基盤
3. Copy
4. Cut
5. Paste + ghost preview
6. 選択/clipboardの移動
7. 90度回転
8. horizontal / vertical flip
9. Undo/Redo統合

maskは巨大矩形を全セル配列として保持しない。通常矩形はboundsでlazyに表現し、生存セルmask等は疎なセル集合として扱う方向。

## その他の候補

- Life rule切替
- generation history / rewind
- coordinate jump/search
- RLE import/export強化
- simulation speed/UI改善
- より高度なstep algorithm（必要になった時点で再評価）

HashLife等はREADME初期構想にはあるが、現時点で実装要件として確定しているわけではない。
