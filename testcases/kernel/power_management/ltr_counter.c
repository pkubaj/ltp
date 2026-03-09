// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2026 Intel - http://www.intel.com/
 */

/*\
 * Validate that ltr_blocker_show rises.
 *
 * Test runs a CPU workload and then checks whether ltr_blocker_show
 * is higher than initial ltr_blocker_show value.
 */

#include "tst_safe_stdio.h"
#include "tst_test.h"
#include "tst_timer_test.h"

static char *path_ltr_blocker_show = "/sys/kernel/debug/pmc_core/ltr_blocker_show";
static int initial_ltr_blocker_show, final_ltr_blocker_show;

static void setup(void)
{
	SAFE_FILE_PRINTF(path_ltr_blocker_show, "%d", initial_ltr_blocker_show);
}

static void *cpu_workload(double run_time)
{
	tst_timer_start(CLOCK_MONOTONIC);
	int num = 2;
	while (!tst_timer_expired_ms(run_time * 1000)) {
		for (int i = 2; i * i <= num; i++) {
			if (num % i == 0)
				break;
		}
		num++;
		SAFE_FILE_SCANF(path_ltr_blocker_show, "%d", &final_ltr_blocker_show);
		if (final_ltr_blocker_show > initial_ltr_blocker_show)
			break;
	}
	return NULL;
}

static void run(void)
{
	double run_time = 30;
	cpu_workload(run_time);
	SAFE_FILE_PRINTF(path_ltr_blocker_show, "%d", initial_ltr_blocker_show);
	if (final_ltr_blocker_show > initial_ltr_blocker_show)
		tst_res(TPASS, "Test pass");
	else
		tst_res(TFAIL, "Final ltr_blocker_show value is not bigger than initial ltr_blocker_show");
}

static struct tst_test test = {
	.min_kver = "6.19",
	.needs_root = 1,
	.supported_archs = (const char *const []) {
		"x86",
		"x86_64",
		NULL
	},
	.setup = setup,
	.test_all = run
};
