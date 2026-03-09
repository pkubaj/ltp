// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2026 Piotr Kubaj <piotr.kubaj@intel.com>
 */

/*\
 * Validate that pkgc_blocker_residency_show shows appropriate counters.
 */

#include <ctype.h>
#include "tst_safe_stdio.h"
#include "tst_test.h"

static const char * const names[] = {
	"misc",
	"ipu_busy",
	"ipu_ltr",
	"ipu_timer",
	"disp_busy",
	"disp_ltr",
	"disp_timer",
	"vpu_busy",
	"vpu_ltr",
	"vpu_timer",
	"pmc_busy",
	"pmc_ltr",
	"pmc_timer",
	"hubatom_arat",
	"cdie0_arat",
	"cdie1_arat",
	"cdie2_arat",
	"cdie3_arat",
	"cdie4_arat",
	"cdie5_arat",
	"gt_arat",
	"media_arat",
	"demotion",
	"thermals",
	"sncu",
	"svtu",
	"iaa",
	"ioc"
};

static unsigned long long counters[ARRAY_SIZE(names)];
static int seen[ARRAY_SIZE(names)];
static char tokens[ARRAY_SIZE(names)][64];

static void setup(void)
{
	for (int i = 0; i < (int)ARRAY_SIZE(names); i++) {
		const char *name = names[i];
		char *out = tokens[i];
		int outsz = sizeof(tokens[i]);
		int n = snprintf(out, outsz, "PKGC_BLOCK_RESIDENCY_");

		for (int j = 0; name[j] && n + 1 < outsz; j++)
			out[n++] = toupper((unsigned char)name[j]);
		out[n] = '\0';
	}
}

static void run(void)
{
	char line[128];
	FILE *fp = SAFE_FOPEN("/sys/kernel/debug/pmc_core/pkgc_blocker_residency_show", "r");
	int missing = 0;

	memset(seen, 0, sizeof(seen));

	while (fgets(line, sizeof(line), fp)) {
		char tmp[128];

		for (int i = 0; i < (int)ARRAY_SIZE(names); i++) {
			if (strstr(line, tokens[i])) {
				SAFE_SSCANF(line, "%127s %llu", tmp, &counters[i]);
				seen[i] = 1;
				break;
			}
		}
	}
	SAFE_FCLOSE(fp);

	for (int i = 0; i < (int)ARRAY_SIZE(names); i++) {
		if (!seen[i]) {
			tst_res(TFAIL, "%s not present", names[i]);
			missing++;
		}
	}
	if (!missing)
		tst_res(TPASS, "All counters are present");
}

static struct tst_test test = {
	.min_kver = "7.1",
	.needs_root = 1,
	.needs_kconfigs = (const char *const []) {
		"CONFIG_INTEL_PMC_CORE",
		NULL
	},
	.supported_archs = (const char *const []) {
		"x86",
		"x86_64",
		NULL
	},
	.setup = setup,
	.test_all = run
};
