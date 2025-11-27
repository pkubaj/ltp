// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Piotr Kubaj <piotr.kubaj@intel.com>
 */

/*\
 * Verify for all online logical CPUs that their highest performance value are
 * the same for HWP Capability MSR 0x771 and CPPC sysfs file.
 */

#include "tst_test.h"
#include "tst_safe_prw.h"

#define MSR_HWP_CAPABILITIES	0x771
#define HIGHEST_PERF_MASK	0xFF

static int nproc;

static void setup(void)
{
	nproc = tst_ncpus_conf();
}

static void run(void)
{
	bool status = true;
	char path[PATH_MAX];

	for (int i = 0; i < nproc; i++) {
		int online = 1;
		unsigned long long msr_highest_perf = 0, sysfs_highest_perf = 0;

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/online", i);
		if (i)
			SAFE_FILE_SCANF(path, "%d", &online);

		if (!online) {
			tst_res(TINFO, "CPU%d offline, skipping", i);
			continue;
		}

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/acpi_cppc/highest_perf", i);
		if (access(path, F_OK) == -1) {
			tst_res(TCONF | TERRNO, "CPPC sysfs not available, skipping");
			return;
		}

		SAFE_FILE_SCANF(path, "%llu", &sysfs_highest_perf);
		tst_res(TDEBUG, "%s: %llu", path, sysfs_highest_perf);

		snprintf(path, sizeof(path), "/dev/cpu/%d/msr", i);
		int fd = SAFE_OPEN(path, O_RDONLY);

		SAFE_PREAD(1, fd, &msr_highest_perf, sizeof(msr_highest_perf), MSR_HWP_CAPABILITIES);
		SAFE_CLOSE(fd);
		msr_highest_perf &= HIGHEST_PERF_MASK;
		tst_res(TDEBUG, "%s: %llu", path, msr_highest_perf);

		if (msr_highest_perf != sysfs_highest_perf) {
			tst_res(TINFO, "cpu%d: sysfs=%llu MSR=%llu",
				i, sysfs_highest_perf, msr_highest_perf);
			status = false;
		}
	}

	if (status)
		tst_res(TPASS, "Sysfs and MSR values are equal");
	else
		tst_res(TFAIL, "Highest performance values differ between sysfs and MSR");
}

static struct tst_test test = {
	.needs_kconfigs = (const char *const []) {
		"CONFIG_ACPI_CPPC_LIB",
		"CONFIG_X86_MSR",
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
