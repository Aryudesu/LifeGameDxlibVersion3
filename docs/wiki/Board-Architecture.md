# Sparse Chunk / step() Architecture

## Chunk

1 Chunk = 64x64 cells。各rowをuint64で表現するため、Life計算をセル単位ではなくbit parallelに処理できる。

Chunk mapは盤面全体を確保せず、生存セル周辺だけを扱う。負座標を含むため、セル座標→Chunk/local座標変換は重要なinvariant。

## Candidate

現世代Chunkの周囲3x3に、次世代で生存セルが現れる可能性がある。この対象をCandidateとして構築する。

現在の高速化後の構造は:

- Candidate本体: dense `std::vector<Candidate>`
- Candidateはcoord + 3x3 `CandidateNeighborhood`
- coord → dense index: flat open-addressing `CandidateIndexSlot` table
- indexは負荷率50%未満を維持し、必要時に2倍grow

Candidateそのものを巨大flat tableへ置いた#28は、slotが大きく初期化・走査量が増えたため不採用。#29で「大きいNeighborhoodはdense、hash側はcoord/indexだけ」に分離して改善した。#33でindexの動的growを追加し、疎で特殊な配置でも固定容量が詰まらないようにした。

## Row materialization

#23以降、Candidateのwest/center/east Chunkから必要なrowを連続したlocal arrayへmaterializeして、hot loop中の参照を軽量化している。

## Neighbor count

#24以降は8近傍をcarry-save adder (CSA) treeでbit-sliced加算する。

最終的に各bit位置についてneighbor countのones/twos/foursを作り、B3/S23を64 cells同時に判定する。

## Correctness優先

step()は最も最適化が集中した箇所。構造変更時はF9 self-testを先に通し、その後benchmarkする。性能だけ良くてもcorrectnessが崩れる変更は採用しない。
