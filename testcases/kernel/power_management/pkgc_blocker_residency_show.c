// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2026 Piotr Kubaj <piotr.kubaj@intel.com>
 */

/*\
 * Validate that pkgc_blocker_residency_show reports package C-state blocker
 * residency counters in the expected "NAME VALUE" format.
 *
 * The kernel prints one line per counter as "%-30s %-30u", where NAME is a
 * PKGC_BLOCK_RESIDENCY_<SOURCE> token and VALUE is the residency the source
 * accumulated while keeping the platform out of package C-states.
 *
 * The actual set of counters is platform specific (each SoC exposes only its
 * own sources), so rather than checking for a fixed list this test verifies
 * that every line names a valid source and carries a numeric value, and that
 * at least one counter is reported.
 *
 * The debugfs file only appears once the intel_pmc_core driver has probed the
 * platform's PMC; when it is absent the test is not applicable (TCONF).
 */

#include <stdio.h>
#include "tst_safe_stdio.h"
#include "tst_test.h"

#define PATH "/sys/kernel/debug/pmc_core/pkgc_blocker_residency_show"
#define PREFIX "PKGC_BLOCK_RESIDENCY_"
#define INVALID PREFIX "INVALID"

enum line_verdict {
	LINE_OK,	/* a valid, countable residency counter */
	LINE_SKIP,	/* ignore this line (e.g. blank) */
	LINE_BAD,	/* malformed or invalid counter -> failure */
};

/*
 * Classify a single line read from the debugfs file.
 *
 * @name:  first whitespace-delimited token (the counter name), or "" when the
 *         line held no token at all
 * @nvals: number of fields sscanf() matched against "%127s %llu" -- 2 means a
 *         name and a numeric residency value were both present
 */
static enum line_verdict check_line(const char *name, int nvals)
{
	if (nvals <= 0)
		return LINE_SKIP;
	if (nvals == 1)
		return LINE_BAD;
	if (strncmp(name, PREFIX, sizeof(PREFIX) - 1) != 0)
		return LINE_BAD;
	if (strcmp(name, INVALID) == 0)
		return LINE_BAD;

	return LINE_OK;
}

static void run(void)
{
	char line[256];
	FILE *fp;
	int seen = 0, bad = 0;

	if (access(PATH, R_OK))
		tst_brk(TCONF, "%s not present (intel_pmc_core not probed?)", PATH);

	fp = SAFE_FOPEN(PATH, "r");

	while (fgets(line, sizeof(line), fp)) {
		char name[128] = "";
		unsigned long long value;
		int nvals = sscanf(line, "%127s %llu", name, &value);

		switch (check_line(name, nvals)) {
		case LINE_OK:
			seen++;
			break;
		case LINE_BAD:
			tst_res(TFAIL, "malformed counter line: %s", line);
			bad++;
			break;
		case LINE_SKIP:
			break;
		}
	}
	SAFE_FCLOSE(fp);

	if (!seen)
		tst_res(TFAIL, "no residency counters reported");
	else if (!bad)
		tst_res(TPASS, "%d residency counters reported, all well-formed", seen);
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
	.test_all = run
};
