#ifndef SQLPROC_PLANNER_H
#define SQLPROC_PLANNER_H

#include "sqlproc/binder.h"
#include "sqlproc/plan.h"

SqlStatus planner_build_script(const BoundScript *bound, PlanScript *out_plan, SqlError *err);

#endif
