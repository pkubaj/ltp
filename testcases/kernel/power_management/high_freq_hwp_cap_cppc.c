// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2025-2026 Intel - http://www.intel.com/
 */

/*\
 * Verify for all online logical CPUs  that their highest performance value are
 * the same for HWP Capability MSR 0x771 and CPPC sysfs file
 */

#include "tst_test.h"

static int nproc;

static void setup(void)
{
	nproc = tst_ncpus();
}

static void run(void)
{
	bool status = 1;

	for (int i = 0; i < nproc; i++) {
		char path[NAME_MAX];
		unsigned long long msr_highest_perf = 0, sysfs_highest_perf = 0;

		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/acpi_cppc/highest_perf", i);
		SAFE_FILE_SCANF(path, "%llu", &sysfs_highest_perf);
		tst_res(TDEBUG, "%s: %llu", path, sysfs_highest_perf);

		snprintf(path, NAME_MAX, "/dev/cpu/%d/msr", i);
		int fd = SAFE_OPEN(path, O_RDONLY);

		if (pread(fd, &msr_highest_perf, sizeof(msr_highest_perf), 0x771) < 0) {
			SAFE_CLOSE(fd);
			tst_brk(TBROK | TERRNO, "MSR read error");
		}
		SAFE_CLOSE(fd);
		msr_highest_perf &= (1ULL << 8) - 1;
		tst_res(TDEBUG, "%s: %llu", path, msr_highest_perf);

		if (msr_highest_perf != sysfs_highest_perf) {
			tst_res(TFAIL, "CPU %d: highest performance values differ between sysfs and MSR", i);
			status = 0;
		}
	}

	if (status)
		tst_res(TPASS, "Test pass");
}

static struct tst_test test = {
	.needs_drivers = (const char *const []) {
		"msr",
		NULL
	},
	.needs_root = 1,
	.setup = setup,
	.supported_archs = (const char *const []) {
		"x86",
		"x86_64",
		NULL
	},
	.test_all = run
};
