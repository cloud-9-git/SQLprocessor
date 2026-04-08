#ifndef SQLPROC_PLANNER_H
#define SQLPROC_PLANNER_H

#include "sqlproc/binder.h"
#include "sqlproc/plan.h"

/* 🧭 바인딩된 statement를 논리 실행 계획(PlanScript)으로 바꿉니다. */
SqlStatus planner_build_script(const BoundScript *bound, PlanScript *out_plan, SqlError *err);

#endif
