// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Intel - http://www.intel.com/Expand commentComment on line R3Resolved
 * Copyright (C) 2026 Tomasz Ossowski <tomasz.ossowski@intel.com>
 */

/*\
 * Validate that the platform presents pkgc_ltr_blocker_show data
 */

#include "tst_safe_stdio.h"
#include "tst_test.h"

#define PKGC_LTR_BLOCKER_PATH "/sys/kernel/debug/pmc_core/pkgc_ltr_blocker_show"

static void run(void)
{
	char line[PATH_MAX];
	int number_elements_found = 0;
	static const char * const list_params[] = {"PKGC_PREVENT_LTR_IADOMAIN",
												"PKGC_PREVENT_LTR_GDIE",
												"PKGC_PREVENT_LTR_PCH",
												"PKGC_PREVENT_LTR_DISPLAY",
												"PKGC_PREVENT_LTR_IPU",
												NULL};

	int number_params = ARRAY_SIZE(list_params) - 1;
	FILE *fp = SAFE_FOPEN(PKGC_LTR_BLOCKER_PATH, "r");

	while (fgets(line, sizeof(line), fp)) {
		for (int i = 0; i < number_params; i++) {
			int value;

	        if (found[i])
    	            continue;

			if (strncmp(line, list_params[i], strlen(list_params[i])))
				continue;

			if (sscanf(line, "%*[^0-9]%d", &value) != 1)
    			tst_res(TFAIL, "Cannot parse value for %s", list_params[i]);

			if(value < 0)
				tst_res(TFAIL, "Negative value for %s: %d", list_params[i], value);
			
  			found[i] = true;
			number_elements_found++;

			tst_res(TINFO, "Found %s = %d",
            	pkgc_params[i], value);

		}
	}
	SAFE_FCLOSE(fp);

	if (number_elements_found == number_params) {
		tst_res(TPASS, "Found all parameters PKGC_PREVENT_LTR_*");
		return;
	}

	tst_res(TFAIL, "Not all PKGC_PREVENT_LTR parameters exist %d / %d", number_elements_found, number_params);
}

static struct tst_test test = {
	.min_runtime = 10,
	.supported_archs = (const char *const[]) {
		"x86_64",
		 NULL
	},
	.min_kver = "6.19",
	.test_all = run,
};
