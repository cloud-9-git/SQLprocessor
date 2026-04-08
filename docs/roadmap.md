# Roadmap

## Stage 1
- 현재 구현
- `INSERT`, `SELECT`, single `WHERE`, file-based text storage

## Stage 2
- `UPDATE`, `DELETE`
- `AND` / `OR`
- `LIMIT`
- planner에 추가 plan node 도입

## Stage 3
- `ORDER BY`
- `EXPLAIN`
- renderer와 planner 고도화

## Stage 4
- `IndexScan`
- secondary index metadata
- storage abstraction 뒤에서 바이너리 페이지 포맷 도입

## Stage 5
- 간단한 optimizer
- 통계 정보
- 더 효율적인 scan 선택
