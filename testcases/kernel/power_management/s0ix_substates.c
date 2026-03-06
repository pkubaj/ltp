// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2026 Intel - http://www.intel.com/
 */

/*\
 * Validate that reading S0IX substates for each PMC die works.
 */

#include "tst_safe_stdio.h"
#include "tst_test.h"

static void run(void)
{
	char line[256];
	FILE *fp = SAFE_FOPEN("/sys/kernel/debug/pmc_core/substate_residencies", "r");

	if (fgets(line, sizeof(line), fp)) {
		if (strstr(line, "pmc0	Substate	Residency"))
			tst_res(TPASS, "Test pass");
		else
			tst_res(TFAIL, "pmc0 string not found in /sys/kernel/debug/pmc_core/substate_residencies");
	}
	SAFE_FCLOSE(fp);
}

static struct tst_test test = {
	.min_kver = "6.19",
	.needs_root = 1,
	.supported_archs = (const char *const []) {
		"x86",
		"x86_64",
		NULL
	},
	.test_all = run
};
