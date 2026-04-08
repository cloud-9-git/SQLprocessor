#!/usr/bin/env python
# -*- coding: utf-8 -*-

from __future__ import annotations

import re
from html import escape
from pathlib import Path


SOURCE_FILES = [
    "types.h",
    "lexer.h",
    "parser.h",
    "executor.h",
    "lexer.c",
    "parser.c",
    "executor.c",
    "main.c",
]

OUT_FILE = Path("sqlsprocessor_line_by_line_ko.html")


MACRO_ROLE = {
    "MAX_RECORDS": "한 테이블에서 관리할 수 있는 행 개수의 최대치입니다. 25000개를 초과하지 않도록 제한합니다.",
    "MAX_COLS": "한 행에서 사용할 수 있는 컬럼 개수 최대치입니다. 기본 파서/실행 로직이 15개를 기준으로 동작합니다.",
    "MAX_TABLES": "메모리에 동시에 열어둘 수 있는 테이블 파일 개수 상한입니다.",
    "MAX_UKS": "하나의 테이블에서 UK(고유값) 컬럼 개수의 최대치입니다.",
    "MAX_SQL_LEN": "한 SQL 문장을 읽을 때 버퍼가 가질 수 있는 최대 길이입니다.",
}

TYPE_ROLE = {
    "StatementType": "SQL 종류를 분류하는 enum 타입입니다. SELECT/INSERT/UPDATE/DELETE를 구분합니다.",
    "ColumnType": "컬럼 제약을 표현하는 enum 타입입니다. 일반, PK, UK, NN 상태를 구분합니다.",
    "Statement": "파서가 한 SQL 문장을 분석해 결과를 담는 구조체입니다.",
    "ColumnInfo": "컬럼 이름과 제약 정보를 묶는 구조체입니다.",
    "TableCache": "CSV 파일을 메모리에 캐시해서 반복 사용하기 위한 구조체입니다.",
    "TokenType": "어휘 분석기에서 만든 토큰 분류입니다.",
    "Token": "현재 토큰 종류와 실제 문자열을 담는 구조체입니다.",
    "Lexer": "SQL 문자열에서 현재 위치를 기억하는 어휘 분석 상태를 저장합니다.",
    "Parser": "Lexer 상태와 현재 토큰을 묶어서 파싱 상태를 관리합니다.",
}

STRUCT_MEMBER_ROLE = {
    "Statement": {
        "type": "이 SQL이 INSERT/SELECT/UPDATE/DELETE 중 어떤 명령인지 저장합니다.",
        "table_name": "대상 테이블 이름",
        "row_data": "INSERT에서 VALUES() 안 문자열 전체",
        "set_col": "UPDATE에서 바꿀 컬럼 이름",
        "set_val": "UPDATE에서 바꿀 새 값",
        "where_col": "WHERE에서 비교할 컬럼 이름",
        "where_val": "WHERE에서 비교할 값",
    },
    "ColumnInfo": {
        "name": "컬럼 이름",
        "type": "해당 컬럼 제약 타입",
    },
    "TableCache": {
        "table_name": "테이블 식별용 이름(파일명 기반)",
        "file": "현재 열린 CSV 파일 포인터",
        "cols": "컬럼 메타정보 배열",
        "col_count": "컬럼 개수",
        "pk_idx": "PK 컬럼 인덱스, 없으면 -1",
        "uk_indices": "UK 컬럼들의 인덱스 목록",
        "uk_count": "UK 컬럼 개수",
        "pk_index": "PK 값 정렬/검색용 인덱스 배열",
        "records": "CSV 한 줄 문자열 레코드 모음",
        "record_count": "레코드(행) 개수",
    },
    "Token": {
        "type": "현재 토큰의 종류",
        "text": "현재 토큰 문자열",
    },
    "Lexer": {
        "sql": "분석 대상 문자열 포인터",
        "pos": "현재 읽고 있는 인덱스",
    },
    "Parser": {
        "lexer": "동작 중인 Lexer 상태",
        "current_token": "현재 읽은 토큰",
    },
}

VAR_ROLE = {
    "stmt": "현재 SQL 한 문장의 분석 결과를 담은 Statement 포인터",
    "p": "파서 상태 포인터",
    "tc": "현재 작업 중인 테이블 캐시 포인터",
    "sql": "분석/실행 대상 SQL 문자열",
    "idx": "일반 순회 인덱스",
    "i": "반복문에서 쓰는 인덱스",
    "j": "두 번째 반복문의 인덱스",
    "k": "보조 인덱스",
    "f": "파일 포인터",
    "fp": "파일 포인터",
    "r": "임시 계산 값",
    "token": "현재 토큰",
    "vals": "INSERT 값 문자열 배열",
    "fields": "CSV 분해 결과 필드 배열",
    "col_name": "찾는 컬럼 이름",
    "where_idx": "WHERE 대상 컬럼 인덱스",
    "set_idx": "SET 대상 컬럼 인덱스",
    "count": "개수 누적용 변수",
    "target_count": "UPDATE 대상 행 개수",
    "match_flags": "각 행의 조건 일치 여부 플래그 배열",
    "open_table_count": "현재 열어둔 테이블 개수",
    "open_tables": "열린 테이블 캐시 배열",
    "filename": "CSV 파일명",
    "line": "입력 파일에서 읽은 임시 문자열",
    "row_data": "INSERT 값 문자열 전체",
    "new_id": "새로 들어올 PK 값",
    "new_line": "INSERT로 새로 만든 한 줄",
}

FUNC_ROLE = {
    "init_lexer": "Lexer에 SQL 문자열과 시작 위치를 넣어 파싱 준비를 합니다.",
    "get_next_token": "현재 위치에서 하나의 토큰을 뽑고 다음 위치로 이동합니다.",
    "advance_parser": "파서의 current_token을 다음 토큰으로 갱신합니다.",
    "expect_token": "현재 토큰이 기대한 종류인지 확인하고 통과하면 한 칸 이동합니다.",
    "parse_where_clause": "WHERE col = value 형태를 읽어 Statement에 where 정보 저장을 시도합니다.",
    "parse_select": "SELECT 구문을 해석해 table_name과 where 조건을 채웁니다.",
    "parse_insert": "INSERT 구문을 해석해 table_name과 row_data를 채웁니다.",
    "parse_update": "UPDATE 구문을 해석해 table_name, set 대상, set 값, where 조건을 채웁니다.",
    "parse_delete": "DELETE 구문을 해석해 table_name, where 조건을 채웁니다.",
    "parse_statement": "SQL 앞 토큰에 따라 적절한 파서 함수로 분기해 처리 결과를 반환합니다.",
    "compare_long": "두 long 값 크기 비교를 리턴합니다.",
    "find_in_pk_index": "정렬된 PK 배열에서 값 존재 여부를 이분 탐색으로 찾습니다.",
    "trim_and_unquote": "문자열 공백/따옴표를 제거해 비교/저장용 형태로 정리합니다.",
    "compare_value": "두 값의 문자열을 정규화한 뒤 동등 비교 결과를 반환합니다.",
    "parse_csv_row": "CSV 한 줄을 필드 배열로 분해합니다.",
    "get_col_idx": "컬럼 이름으로 컬럼 인덱스를 찾습니다.",
    "rewrite_file": "캐시 상태를 전체 CSV 파일에 다시 기록합니다.",
    "insert_pk_sorted": "PK 정렬 순서를 유지하며 레코드를 삽입합니다.",
    "get_table": "테이블 캐시를 찾고 없으면 파일에서 헤더/레코드를 읽어 새로 생성합니다.",
    "execute_insert": "INSERT 검증(예: NN/PK/UK) 후 저장소 갱신을 수행합니다.",
    "execute_select": "조건 유무에 따라 전체/필터 조회를 출력합니다.",
    "execute_update": "WHERE 조건 행을 찾아 set 값으로 갱신하고 저장합니다.",
    "execute_delete": "WHERE 조건 행을 삭제하고 남은 행으로 정리해 파일에 반영합니다.",
    "close_all_tables": "열린 모든 파일 포인터를 닫아 자원 정리를 합니다.",
    "main": "입력 SQL 파일을 읽어 ';' 기준으로 문장 분리 후 파싱-실행을 반복합니다.",
}

LIB_FUNC_HINT = {
    "fopen": "표준 파일 열기",
    "fclose": "표준 파일 닫기",
    "fflush": "버퍼 출력 강제 반영",
    "fgets": "한 줄 읽기",
    "fprintf": "파일에 형식 문자열 출력",
    "printf": "표준 출력 출력",
    "snprintf": "크기 제한 문자열 생성",
    "fgetc": "한 글자 읽기",
    "strncpy": "문자열 길이 제한 복사",
    "strchr": "문자열에서 특정 문자 첫 위치 찾기",
    "strrchr": "문자열에서 특정 문자 마지막 위치 찾기",
    "strtok": "문자열 토큰 분리",
    "strncmp": "문자열 앞부분 비교",
    "strpbrk": "문자열에서 특정 문자 집합 검색",
    "strcmp": "문자열 전체 비교",
    "atoi": "문자열을 int로 변환",
    "atol": "문자열을 long으로 변환",
    "isspace": "공백 문자 판별",
    "isalnum": "영문/숫자 판별",
    "toupper": "대문자 변환",
    "bsearch": "정렬 배열 이분탐색",
    "memmove": "메모리 블록 이동",
    "memset": "메모리 초기화",
}


def strip_comment(line: str) -> str:
    idx = line.find("//")
    return line[:idx].rstrip() if idx >= 0 else line.rstrip("\n")


def compact(line: str) -> str:
    return re.sub(r"\s+", " ", line).strip()


def parse_struct_member(line: str):
    m = re.match(
        r"^\s*(?P<typ>[A-Za-z_][A-Za-z0-9_\s\*]+)\s+(?P<name>[A-Za-z_][A-Za-z0-9_]*)(?P<arr>\s*\[[^\]]+\])?\s*;",
        line,
    )
    if not m:
        return None
    return m.group("typ").strip(), m.group("name"), m.group("arr") or ""


def describe_decl(trim: str, current_vars: dict[str, str]) -> str:
    m = re.match(r"^\s*(?:static\s+)?(?:const\s+)?(?P<typ>[A-Za-z_][A-Za-z0-9_\s\*]+)\s+(?P<rest>.+);$", trim)
    if not m:
        return ""

    typ = m.group("typ").strip()
    rest = m.group("rest")
    parts = [p.strip() for p in rest.split(",") if p.strip()]
    messages = []
    for p in parts:
        mm = re.match(r"(?P<name>[A-Za-z_][A-Za-z0-9_]*)(?:\s*=\s*.*)?$", p)
        if not mm:
            continue
        name = mm.group("name")
        role = current_vars.get(name, VAR_ROLE.get(name, f"{typ} 형식 변수"))
        current_vars[name] = role
        messages.append(f"`{name}`: {role}")
    return "변수 선언입니다. " + " / ".join(messages) if messages else ""


def register_params(signature: str, current_vars: dict[str, str]) -> None:
    m = re.match(r"^[A-Za-z_][A-Za-z0-9_\s\*]*\s+[A-Za-z_][A-Za-z0-9_]*\s*\((?P<params>.*)\)", signature)
    if not m:
        return
    params = m.group("params").strip()
    if not params or params == "void":
        return
    for p in params.split(","):
        p = p.strip()
        mm = re.match(r".+\s+([A-Za-z_][A-Za-z0-9_]*)$", p)
        if mm:
            current_vars[mm.group(1)] = VAR_ROLE.get(mm.group(1), "함수 매개변수")


def describe_assignment(trim: str, current_vars: dict[str, str]) -> str:
    m = re.match(r"^\s*(?P<obj>[A-Za-z_][A-Za-z0-9_]*)\s*->\s*(?P<field>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*(?P<rhs>.+?)\s*;\s*$", trim)
    if m:
        obj = m.group("obj")
        field = m.group("field")
        rhs = m.group("rhs")
        obj_role = VAR_ROLE.get(obj, "변수")
        field_role = STRUCT_MEMBER_ROLE.get(obj, {}).get(field, f"`{obj}` 구조체의 `{field}` 항목")
        return f"{obj}({obj_role})의 `{field}`에 `{rhs}`를 저장합니다. 이것은 {field_role}입니다."

    m = re.match(r"^\s*(?P<lhs>[A-Za-z_][A-Za-z0-9_]*)\s*=\s*(?P<rhs>.+)\s*;\s*$", trim)
    if m:
        lhs = m.group("lhs")
        rhs = m.group("rhs")
        role = current_vars.get(lhs, VAR_ROLE.get(lhs, "변수"))
        return f"변수 `{lhs}`에 `{rhs}`를 넣습니다. `{lhs}`의 용도: {role}"
    return ""


def describe_control(trim: str) -> str:
    if re.match(r"^\s*switch\s*\(", trim):
        return "switch 분기문 시작입니다."
    if re.match(r"^\s*case\s+.+:\s*$", trim):
        return "switch에서 해당 case 분기를 처리합니다."
    if re.match(r"^\s*default:\s*$", trim):
        return "switch의 기본(default) 분기를 처리합니다."
    m = re.match(r"^\s*if\s*\((.+)\)\s*$", trim)
    if m:
        return f"if 조건 분기입니다. 조건이 `{m.group(1)}` 일 때 참이면 블록을 실행합니다."
    m = re.match(r"^\s*else if\s*\((.+)\)\s*$", trim)
    if m:
        return f"else if 분기입니다. 이전 조건이 거짓일 때 `{m.group(1)}` 조건을 검사합니다."
    if re.match(r"^\s*else\s*$", trim):
        return "if 계열에서 앞 조건이 모두 거짓일 때 실행되는 else 분기입니다."
    m = re.match(r"^\s*for\s*\((.+)\)\s*$", trim)
    if m:
        return f"for 반복문입니다. `{m.group(1)}` 형태로 반복 횟수와 조건을 제어합니다."
    m = re.match(r"^\s*while\s*\((.+)\)\s*$", trim)
    if m:
        return f"while 반복문입니다. `{m.group(1)}` 조건이 참이면 계속 반복합니다."
    if re.match(r"^\s*return\b", trim):
        m = re.match(r"^\s*return\s*(.*);?\s*$", trim)
        val = m.group(1).strip() if m else ""
        return f"함수를 종료하고 `{val}`를 반환합니다." if val else "함수를 종료합니다."
    if re.match(r"^\s*break;\s*$", trim):
        return "현재 반복문/분기에서 즉시 빠져나갑니다."
    if re.match(r"^\s*continue;\s*$", trim):
        return "현재 반복 단계의 나머지를 건너뛰고 다음 반복으로 넘어갑니다."
    if trim == "{":
        return "블록 시작입니다."
    if trim == "}":
        return "블록 끝입니다."
    return ""


def describe_call(trim: str) -> str:
    if "(" not in trim:
        return ""
    if re.match(r"^\s*(if|while|for|switch)\s*\(", trim):
        return ""
    names = []
    for fn in re.findall(r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(", trim):
        if fn in {"if", "while", "for", "switch", "case", "return"}:
            continue
        if fn in LIB_FUNC_HINT:
            names.append(f"`{fn}`: {LIB_FUNC_HINT[fn]}")
        elif fn in FUNC_ROLE:
            names.append(f"`{fn}`: {FUNC_ROLE[fn]}")
        elif fn not in names:
            names.append(f"`{fn}`: 사용자 정의 함수 호출")
    return " / ".join(dict.fromkeys(names))


def describe_line(trim: str, vars: dict[str, str], in_struct_name: str | None, in_enum: bool) -> str:
    if not trim:
        return "빈 줄"
    if trim.startswith("//"):
        return "한 줄 주석입니다."
    if trim.startswith("/*"):
        return "블록 주석 시작입니다."
    if trim.startswith("*/"):
        return "블록 주석 종료입니다."
    if trim.startswith("#"):
        m = re.match(r"^\s*#define\s+([A-Za-z_][A-Za-z0-9_]*)\s+(.+)$", trim)
        if m:
            name = m.group(1)
            role = MACRO_ROLE.get(name)
            if role:
                return f"상수 `{name}` 선언: {role}"
        return "전처리 지시문입니다."

    if in_enum:
        m = re.match(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*,?\s*;?$", trim)
        if m:
            return f"enum 항목 `{m.group(1)}` 선언입니다."

    if in_struct_name:
        parsed = parse_struct_member(trim)
        if parsed:
            typ, name, arr = parsed
            role = STRUCT_MEMBER_ROLE.get(in_struct_name, {}).get(name, "맵핑되지 않은 멤버")
            return f"구조체 `{in_struct_name}`의 멤버 선언: `{typ}{arr} {name}`. 용도: {role}"

    c = describe_control(trim)
    if c:
        return c

    d = describe_decl(trim, vars)
    if d:
        return d

    d = describe_assignment(trim, vars)
    if d:
        return d

    d = describe_call(trim)
    if d:
        return d

    return "코드 동작 라인입니다. 위/아래 문맥의 변수와 함수 호출 흐름으로 이해하면 됩니다."


def collect_line_signature(trim: str, vars: dict[str, str]) -> str:
    m = re.match(r"^\s*[A-Za-z_][A-Za-z0-9_\s\*]*\s+([A-Za-z_][A-Za-z0-9_]*)\s*\((.*)\)\s*\{?\s*$", trim)
    if not m:
        return ""
    fn = m.group(1)
    register_params(trim, vars)
    return f"함수 `{fn}` 선언입니다. {FUNC_ROLE.get(fn, '용도는 내부 구현으로 판단')}"


def make_summary(src: str) -> str:
    if src == "types.h":
        return "타입과 구조체, enum을 정의해 전체 데이터 형식을 잡는 설계 파일입니다."
    if src == "lexer.h":
        return "lexer 함수 선언만 있는 인터페이스 파일입니다."
    if src == "parser.h":
        return "parser 인터페이스 파일입니다."
    if src == "executor.h":
        return "실행 엔진 함수 선언부입니다."
    if src == "lexer.c":
        return "SQL을 토큰 단위로 쪼개는 어휘 분석기 구현부입니다."
    if src == "parser.c":
        return "토큰 목록을 기반으로 Statement를 구성하는 구문 분석기 구현부입니다."
    if src == "executor.c":
        return "테이블 캐시 관리, 제약 검사, 삽입/조회/갱신/삭제 실행부입니다."
    if src == "main.c":
        return "프로그램 시작점으로 SQL 파일을 읽고 실행하는 진입부입니다."
    return ""


def generate_html() -> None:
    rows: list[str] = []
    rows.append("<!doctype html>")
    rows.append("<html lang='ko'>")
    rows.append("<head>")
    rows.append("<meta charset='UTF-8'>")
    rows.append("<meta name='viewport' content='width=device-width, initial-scale=1'>")
    rows.append("<title>SQLprocessor 줄 단위 상세 주석</title>")
    rows.append("<style>")
    rows.append("body{margin:0;font-family:Consolas,'D2Coding','Malgun Gothic',sans-serif;line-height:1.5;background:#f7fbff;color:#0f172a;}")
    rows.append("header{position:sticky;top:0;background:#0f172a;color:#fff;padding:12px 16px;z-index:3;}")
    rows.append("main{padding:12px;}")
    rows.append("section{margin:12px 0;padding:8px;border:1px solid #e2e8f0;border-radius:10px;background:#fff;}")
    rows.append("table{width:100%;border-collapse:collapse;table-layout:fixed;font-size:13px;}")
    rows.append("th,td{border-bottom:1px solid #dbe3ef;padding:8px 10px;vertical-align:top;word-break:break-word;}")
    rows.append(".line{width:60px;}")
    rows.append(".code{width:44%;background:#f1f5f9;white-space:pre;}")
    rows.append(".desc{width:56%;white-space:pre-wrap;}")
    rows.append("thead th{background:#e2e8f0;position:sticky;top:76px;}")
    rows.append(".note{margin:0 0 8px;color:#334155;}")
    rows.append("</style>")
    rows.append("</head>")
    rows.append("<body>")
    rows.append("<header><h1>SQLprocessor 줄 단위 상세 주석</h1><p class='note'>한 줄 한 줄에 '무슨 용도인지'를 붙여 설명합니다.</p></header><main>")

    for src in SOURCE_FILES:
        p = Path(src)
        if not p.exists():
            continue

        lines = p.read_text(encoding="utf-8", errors="ignore").splitlines()
        rows.append(f"<section><h2>{src}</h2><p class='note'>{escape(make_summary(src))}</p>")
        rows.append("<table><thead><tr><th class='line'>Line</th><th class='code'>Code</th><th class='desc'>설명</th></tr></thead><tbody>")

        in_struct = None  # None | struct name | "__unknown__"
        in_enum = False
        struct_member_rows: list[tuple[int, str, str]] = []
        brace_depth = 0
        current_vars: dict[str, str] = {}
        file_entries: list[tuple[int, str, str]] = []

        for idx, raw in enumerate(lines, start=1):
            line = raw.rstrip("\n")
            trim = compact(strip_comment(line))
            desc = ""

            if re.match(r"^\s*typedef\s+struct\s*\{$", trim):
                in_struct = "__unknown__"
                struct_member_rows = []
                desc = "구조체 정의 시작입니다."
            elif re.match(r"^\s*typedef\s+enum\s*\{$", trim):
                in_enum = True
                desc = "enum 정의 시작입니다."
            elif in_struct is not None and re.match(r"^\s*}\s*[A-Za-z_][A-Za-z0-9_]*\s*;\s*$", trim):
                m = re.match(r"^\s*}\s*([A-Za-z_][A-Za-z0-9_]*)\s*;\s*$", trim)
                struct_name = m.group(1) if m else "Unknown"
                base_desc = TYPE_ROLE.get(struct_name, "구조체")
                if struct_name == "Statement":
                    sub = " 이 구조체는 SQL 한 줄의 핵심 정보(type, 대상 테이블, where/set 값)를 담는 상자입니다."
                elif struct_name == "TableCache":
                    sub = " 하위로 메타 정보(테이블/제약), 컬럼 정의, 실제 레코드 저장/인덱스 역할을 묶어 해석하면 됩니다."
                else:
                    sub = ""
                desc = f"`{struct_name}` 구조체 정의가 끝났습니다. 용도: {base_desc}.{sub}"

                # 구조체 멤버 설명 갱신
                for entry_idx, field_name, field_type in struct_member_rows:
                    role = STRUCT_MEMBER_ROLE.get(struct_name, {}).get(field_name)
                    if role:
                        file_entries[entry_idx][2] = f"구조체 `{struct_name}`의 멤버 선언: `{field_type} {field_name}`. 용도: {role}"
                in_struct = None
                struct_member_rows = []
            elif in_enum and re.match(r"^\s*}\s*[A-Za-z_][A-Za-z0-9_]*\s*;\s*$", trim):
                m = re.match(r"^\s*}\s*([A-Za-z_][A-Za-z0-9_]*)\s*;\s*$", trim)
                enum_name = m.group(1) if m else "Unknown"
                if enum_name == "StatementType":
                    desc = "StatementType enum 정의가 끝납니다. 파서에서 SQL 종류 판별에 사용됩니다."
                elif enum_name == "ColumnType":
                    desc = "ColumnType enum 정의가 끝납니다. 컬럼 제약(COL_PK/COL_UK/COL_NN) 판별에 사용됩니다."
                elif enum_name == "TokenType":
                    desc = "TokenType enum 정의가 끝납니다. 어휘 분석 토큰의 종류를 분류합니다."
                else:
                    desc = f"`{enum_name}` enum 정의가 끝납니다."
                in_enum = False
            else:
                if re.match(r"^\s*[A-Za-z_][A-Za-z0-9_\s\*]+\s+[A-Za-z_][A-Za-z0-9_]+\s*\(.*\)\s*\{?\s*$", trim):
                    desc = collect_line_signature(trim, current_vars)
                else:
                    desc = describe_line(trim, current_vars, struct_name if (struct_name := in_struct) else None, in_enum)

                if in_struct is not None and in_struct != "__unknown__":
                    parsed = parse_struct_member(trim)
                    if parsed:
                        typ, name, arr = parsed
                        struct_member_rows.append((len(file_entries), name, f"{typ}{arr}"))
                elif in_struct == "__unknown__":
                    parsed = parse_struct_member(trim)
                    if parsed:
                        typ, name, arr = parsed
                        struct_member_rows.append((len(file_entries), name, f"{typ}{arr}"))

            file_entries.append([idx, line, desc])

            # 함수 범위 벗어나면 지역변수 추적 초기화
            brace_depth += line.count("{") - line.count("}")
            if trim.startswith("}") and brace_depth <= 0:
                current_vars = {}
                brace_depth = 0

        for idx, code_line, desc in file_entries:
            rows.append(f"<tr><td class='line'>{idx}</td><td class='code'>{escape(code_line)}</td><td class='desc'>{escape(desc)}</td></tr>")

        rows.append("</tbody></table></section>")

    rows.append("</main></body></html>")
    OUT_FILE.write_text("\n".join(rows), encoding="utf-8-sig")
    print(f"generated: {OUT_FILE.resolve()}")


if __name__ == "__main__":
    generate_html()
