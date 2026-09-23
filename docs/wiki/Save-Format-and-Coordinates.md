# Save format / Coordinates

## .ary3

`InfiniteLifeFile` の現行format:
- magic: `ARYLIFE3`
- formatVersion: 1
- generation: uint64
- alive cell count: uint64
- checksum: FNV-1a 32bit
- payload: 生存セルごとに signed 64bit x/y（bit patternをlittle endian uint64として保存）

1 cellあたり16 bytes。payload上限は256 MiB。

保存時は一旦 `.tmp` を書き、`MoveFileExA(... MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` で置換する。

## Load integrity

Loadは直接現在盤面を壊さず、読み込み結果の整合性を確認してから置換する。異常時は `.diagnostic.log` を生成する。

## 負座標境界の注意

過去に -64 / -128 等のChunk境界付近で、Load後にセルが隣Chunkへずれるように見える断続的症状を調査した。

PR #12: 診断追加（未merge）
PR #14: 負座標のChunk/local分解を単一ロジックへ統一（merge）
PR #15: Load integrity guard（merge）

重要: 症状はhardeningされたが、過去の断続的現象について「根本原因を完全に特定・修正した」とは扱わない。Release/LTCG miscompile説やUB/メモリ破壊説は仮説のまま。

座標分解を変更する場合は、特に負のChunk境界を重点的にtestする。
