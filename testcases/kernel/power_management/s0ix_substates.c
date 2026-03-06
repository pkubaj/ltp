// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2026 Intel - http://www.intel.com/
 */

/*\
 * Validate that reading S0IX substates for each PMC die works.
 */

#include <string.h>
#include "tst_safe_stdio.h"
#include "tst_test.h"

#define MAX_PMC_DIES 16
#define PATH_SUBSTATE_RESIDENCIES "/sys/kernel/debug/pmc_core/substate_residencies"

static void run(void)
{
	char line[256];
	int pmc_count = 0;
	int substate_count = 0;
	int current_pmc = -1;
	int found_pmc[MAX_PMC_DIES] = {0};
	int substates_per_pmc[MAX_PMC_DIES] = {0};
	FILE *fp = SAFE_FOPEN(PATH_SUBSTATE_RESIDENCIES, "r");

	while (fgets(line, sizeof(line), fp)) {
		int pmc_num;

		/* Check for PMC die header (e.g., "pmc0   Substate       Residency") */
		if (sscanf(line, "pmc%d", &pmc_num) == 1 && strstr(line, "Substate")) {
			if (pmc_num >= MAX_PMC_DIES) {
				tst_res(TWARN, "PMC die number %d exceeds maximum %d",
					pmc_num, MAX_PMC_DIES - 1);
				continue;
			}

			if (found_pmc[pmc_num]) {
				tst_res(TFAIL, "Duplicate PMC die pmc%d found", pmc_num);
				SAFE_FCLOSE(fp);
				return;
			}

			found_pmc[pmc_num] = 1;
			current_pmc = pmc_num;
			pmc_count++;
			tst_res(TPASS, "Found PMC die pmc%d header", pmc_num);
			continue;
		}

		/* Check for substate entries (e.g., "         S0i2.0               0") */
		if (current_pmc >= 0 && strstr(line, "S0i2.")) {
			char substate[32];
			unsigned long long residency;

			if (SAFE_SSCANF(line, " %31s %llu", substate, &residency) == 2) {
				substates_per_pmc[current_pmc]++;
				substate_count++;
				tst_res(TPASS, "pmc%d: Found substate %s with residency %llu",
					current_pmc, substate, residency);
			}
		}
	}

	SAFE_FCLOSE(fp);

	for (int i = 0; i < MAX_PMC_DIES; i++) {
		if (found_pmc[i] && substates_per_pmc[i] == 0) {
			tst_res(TFAIL, "pmc%d header found but no substates listed", i);
			return;
		}
	}

	if (pmc_count == 0)
		tst_res(TFAIL, "No PMC dies found in %s", PATH_SUBSTATE_RESIDENCIES);
	else
		tst_res(TPASS, "Successfully validated %d PMC die(s) with %d total substate(s)",
			pmc_count, substate_count);
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
