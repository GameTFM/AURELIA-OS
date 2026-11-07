/* AURELIA OS - cooperative kernel thread context-switch foundation */
#pragma once

#include <aurelia/types.h>

/* Runs two kernel threads that yield in round-robin order. This is a
   context-switch self-test, not the final preemptive scheduler. */
bool sched_selftest(void);
