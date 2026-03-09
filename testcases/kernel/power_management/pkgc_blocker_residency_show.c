// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2026 Intel - http://www.intel.com/
 */

/*\
 * Validate that pkgc_blocker_residency_show rises.
 */

#include "tst_safe_stdio.h"
#include "tst_test.h"
#include "tst_timer_test.h"

static uint64_t initial_counters[27], final_counters[27];

const char * const names[] = {
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

static void read_pkgc_blocker_residencies (uint64_t *counters)
{
	char line[128];
	FILE *fp = SAFE_FOPEN("/sys/kernel/debug/pmc_core/pkgc_blocker_residency_show", "r");
	while (fgets(line, sizeof(line), fp)) {
		char tmp[128];

		if (strstr(line, "PKGC_BLOCK_RESIDENCY_MISC"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[0]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_IPU_BUSY"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[1]);
//		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_IPU_LTR"))
//			SAFE_SSCANF(line, "%s %lu", tmp, &counters[2]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_IPU_LTRPKGC_BLOCK_RESIDENCY_IPU_TIMER"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[2]);
//		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_IPU_TIMER"))
//			SAFE_SSCANF(line, "%s %lu", tmp, &counters[3]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_DISP_BUSY"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[3]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_DISP_LTR"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[4]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_DISP_TIMER"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[5]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_VPU_BUSY"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[6]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_VPU_LTR"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[7]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_VPU_TIMER"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[8]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_PMC_BUSY"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[9]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_PMC_LTR"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[10]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_PMC_TIMER"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[11]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_HUBATOM_ARAT"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[12]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_CDIE0_ARAT"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[13]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_CDIE1_ARAT"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[14]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_CDIE2_ARAT"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[15]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_CDIE3_ARAT"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[16]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_CDIE4_ARAT"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[17]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_CDIE5_ARAT"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[18]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_GT_ARAT"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[19]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_MEDIA_ARAT"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[20]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_DEMOTION"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[21]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_THERMALS"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[22]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_SNCU"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[23]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_SVTU"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[24]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_IAA"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[25]);
		else if (strstr(line, "PKGC_BLOCK_RESIDENCY_IOC"))
			SAFE_SSCANF(line, "%s %lu", tmp, &counters[26]);
	}
	SAFE_FCLOSE(fp);
}

static void *cpu_workload(const double run_time)
{
	tst_timer_start(CLOCK_MONOTONIC);
	int num = 2;
	while (!tst_timer_expired_ms(run_time * 1000)) {
		for (int i = 2; i * i <= num; i++) {
			if (num % i == 0)
				break;
		}
		num++;
//		SAFE_FILE_SCANF(path_pkgc_blocker_residency_show, "%d", &final_pkgc_blocker_residency_show);
//		if (final_pkgc_blocker_residency_show > initial_pkgc_blocker_residency_show)
//			break;
	}
	return NULL;
}

static void run(void)
{
	read_pkgc_blocker_residencies(initial_counters);
	for (int i = 0; i < 27; i++)
		tst_res(TINFO, "%s: %lu", names[i], initial_counters[i]);

	cpu_workload(30);
	read_pkgc_blocker_residencies(final_counters);
	for (int i = 0; i < 27; i++)
		tst_res(TINFO, "%s: %lu", names[i], final_counters[i]);

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
