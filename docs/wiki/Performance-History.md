# Performance optimization history

巨大保存データ（約3.4M generation / 約284k alive / 約34.6k chunks付近）を主なfixtureとして実測した。

## 大きな改善

| 段階 | 概要 | おおよその実測 |
|---|---|---:|
| 初期 | Sparse Chunk step | 33-35 gen/s |
| #19 | Candidate生成時に3x3 neighborhood cache | ~44.6 gen/s |
| #23 | row参照を連続配列化 | ~50 gen/s |
| #24 | 8近傍countをCSA tree化 | ~55.5 gen/s |
| #29 | Candidateをdense vector + flat indexへ分離 | ~58-59 gen/s |
| #33 | Candidate indexを動的grow可能に | correctness/hardening |
| 最新main | #33まで | ~60 gen/s |

初期から約1.7倍強。

## 採用しなかった主な実験

- #17 hash軽量化: 決定的改善なし
- #18/#20/#25/#30: profiling専用
- #21 bucket再利用: regression
- #22 map node再利用: 大幅regression
- #26 existing Chunk先行Candidate: regression
- #27 lazy row materialization: regression
- #28 Candidate全体flat table: 約46 gen/s。大きなslotの初期化/走査が重い
- #31 next Chunk metadata local集約: regression
- #32 CSA output mask簡約: source上は簡潔だがregression

## #34-#36の「怪奇現象」

一時期、mainが約49.5 gen/s、reserve関連の実験branchが約60 gen/sとなり、`candidates.reserve()` の位置だけで約20%変化したように見えた。

#35でreserve量を変えても約60、#36でreserve量をmainと同じにして位置だけ動かしても約60だったため、MSVC codegen差を疑ってassembly比較まで進んだ。

そこでmain側asmに古い `unordered_map Candidate / _Try_emplace` 実装が残っていることを発見。ローカルmainがpullされていなかったことが判明した。

最新mainをpull/rebuildすると中央値約59.95 gen/sとなり、#36との差は消滅した。

教訓:
- benchmark前にbranch/SHAとpull状態を確認する
- Release x64でclean/rebuildする
- 不可解な性能差は最終machine code比較が有効
- compiler怪奇現象と断定する前に比較binary/sourceの同一性を確認する
