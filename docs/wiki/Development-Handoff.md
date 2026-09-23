# Development handoff

## 新しい作業スレッドで最初に確認するもの

1. mainをpull済みか確認
2. open PRとそのbase/headを確認
3. `InfiniteLifeBoard.h/.cpp`
4. `Main.cpp`
5. このWiki草案のCurrent-Spec / Board-Architecture / Performance-History
6. 作業対象に応じてEditHistory / PatternLibrary / InfiniteLifeFile等

## Build / test

主な開発環境:
- Visual Studio
- Release x64
- C++ / DxLib

step()や盤面構造を変更したら、まずF9 self-test。

F9 test:
- naive Conway oracleとの完全一致
- seed `0x41525955ULL`
- 5000 random boundary cases
- 各4 generations
- -129,-128,-127,-65,-64,-63,-2,-1,0,1,2,63,64,65,127 等の境界を含む

期待:
`PASS: 5000 random boundary cases x 4 generations matched the naive Conway implementation.`

その後、巨大save fixtureでperformance benchmark。

## Git / PR運用

- mainを直接変更しない
- feature/diagnostic branch + PR
- profiling専用PRや失敗実験はmergeしない
- performance変更は測定結果をPRへ残す
- diagnostic instrumentation自体が性能をperturbする可能性を考慮
- merge前には必要ならF9結果を確認
- 明示的な確認なしにmergeしない

## step()変更時の注意

hot pathは非常にcodegen-sensitive。sourceが短くなることと高速化は一致しない（#32）。

Candidate構造は#29/#33でかなり整理されているため、大規模変更はbenchmark前提。

## Save/coordinate変更時

負の64-cell境界を必ず意識する。過去の断続的Load shift問題は根本原因確定扱いにしない。
