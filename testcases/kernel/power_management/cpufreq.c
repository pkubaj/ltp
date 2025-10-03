// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2025-2026 Intel - http://www.intel.com/
 */

/*\
 * Runs various sanity checks for intel_pstate cpufreq sysfs interface.
 *
 * Checks whether intel_pstate driver is used, whether it is possible to switch
 * scaling_governor to powersave and performance, whether it is possible
 * to disable turbo frequency and whether it is possible to change
 * scaling_min_freq and scaling_max_freq to its minimal valid and maximal valid
 * values.
 */

#include "tst_test.h"

#define	STRING_LEN	23

static char path[NAME_MAX];
static int nproc, previous_scaling_max_freq, previous_scaling_min_freq;

static void cleanup(void)
{
	SAFE_FILE_PRINTF("/sys/devices/system/cpu/intel_pstate/no_turbo", "%d", 0);
	for (int i = 0; i < nproc; i++) {
		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
		SAFE_FILE_PRINTF(path, "%d", previous_scaling_max_freq);

		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq", i);
		SAFE_FILE_PRINTF(path, "%d", previous_scaling_min_freq);
	}
}

static void setup(void)
{
	SAFE_FILE_PRINTF("/sys/devices/system/cpu/intel_pstate/status", "active");
	nproc = tst_ncpus();
}

static void run(void)
{
	const char * const cpufreq_nodes[] = {
		"affected_cpus",
		"cpuinfo_max_freq",
		"cpuinfo_min_freq",
		"cpuinfo_transition_latency",
		"related_cpus",
		"scaling_available_governors",
		"scaling_cur_freq",
		"scaling_driver",
		"scaling_governor",
		"scaling_max_freq",
		"scaling_min_freq",
		"scaling_setspeed",
		NULL
	};
	bool status = 1;
	char contents[STRING_LEN] = {0};
	int fd, cpuinfo_max_freq, cpuinfo_min_freq, scaling_max_freq, scaling_min_freq;

	for (int i = 0; i < nproc; i++) {
		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_driver", i);
		SAFE_FILE_SCANF(path, "%s", contents);
		tst_res(TDEBUG, "Checking whether %s is \"intel_pstate\"", path);
		if (!!strncmp("intel_pstate", contents, STRING_LEN)) {
			tst_res(TINFO, "%s contains: %s", path, contents);
			tst_res(TFAIL, "%s is not intel_pstate", path);
			status = 0;
		}

		for (int j = 0; cpufreq_nodes[j] != NULL; j++) {
			struct stat stats;

			snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/%s", i, cpufreq_nodes[j]);
			tst_res(TDEBUG, "Checking whether %s is a regular file", path);
			if (!stat(path, &stats)) {
				if (!S_ISREG(stats.st_mode)) {
					tst_res(TINFO, "%s mode: %d\n", path, stats.st_mode);
					tst_res(TFAIL, "%s is not a regular file", path);
					status = 0;
				}
			} else
				tst_brk(TCONF, "stat() call on %s failed", path);
		}

		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_available_governors", i);
		fd = SAFE_OPEN(path, O_RDONLY);

		if (SAFE_READ(0, fd, contents, STRING_LEN) != -1) {
			tst_res(TDEBUG, "Checking whether %s is \"performance powersave\"", path);
			if (!!strncmp("performance powersave\n", contents, STRING_LEN)) {
				tst_res(TINFO, "%s contains: %s", path, contents);
				tst_res(TFAIL, "%s is not \"performance powersave\"", path);
				status = 0;
			}
		}
		SAFE_CLOSE(fd);

		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
		tst_res(TDEBUG, "Checking %s", path);
		SAFE_FILE_SCANF(path, "%s", contents);
		if (!!strncmp("performance", contents, STRING_LEN)) {
			tst_res(TDEBUG, "%s is \"performance\"", path);
			tst_res(TDEBUG, "Checking whether %s can be switched to \"powersave\"", path);
			SAFE_FILE_PRINTF(path, "powersave");
			SAFE_FILE_SCANF(path, "%s", contents);
			if (!!strncmp("powersave", contents, STRING_LEN)) {
				tst_res(TFAIL, "In %s, failed to change scaling_governor from performance to powersave, current scaling_governor: %s", path, contents);
				status = 0;
			}

		} else if (!!strncmp("powersave", contents, STRING_LEN)) {
			tst_res(TDEBUG, "%s is \"powersave\"", path);
			tst_res(TDEBUG, "Checking whether %s can be switched to \"performance\"", path);
			SAFE_FILE_PRINTF(path, "performance");
			SAFE_FILE_SCANF(path, "%s", contents);
			if (!!strncmp("performance", contents, STRING_LEN)) {
				tst_res(TFAIL, "In %s, failed to change scaling_governor from powersave to performance, current scaling_governor: %s", path, contents);
				status = 0;
			}

		} else
			tst_brk(TBROK, "Unknown scaling_governor: %s", contents);

		uint32_t cpuinfo_min_freq = 0, cpuinfo_max_freq = 0;
		char *path_cpuinfo_min_freq = malloc(sizeof(char) * NAME_MAX);

		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);
		snprintf(path_cpuinfo_min_freq, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_min_freq", i);
		tst_res(TDEBUG, "Checking whether %s is bigger than cpuinfo_min_freq", path);
		SAFE_FILE_SCANF(path, "%d", &cpuinfo_max_freq);

		SAFE_FILE_SCANF(path_cpuinfo_min_freq, "%d", &cpuinfo_min_freq);
		free(path_cpuinfo_min_freq);

		if (cpuinfo_min_freq > cpuinfo_max_freq) {
			tst_res(TFAIL, "Value in %s: %d, should be bigger than cpuinfo_min_freq: %d", path, cpuinfo_max_freq, cpuinfo_min_freq);
			status = 0;
		}
	}

	snprintf(path, NAME_MAX, "/sys/devices/system/cpu/intel_pstate/status");
	tst_res(TDEBUG, "Checking whether %s contains \"active\"", path);
	SAFE_FILE_SCANF(path, "%s", contents);

	if (!!strncmp("active", contents, STRING_LEN)) {
		tst_res(TFAIL, "%s is not \"active\", but %s", path, contents);
		status = 0;
	}

	tst_res(TDEBUG, "Checking whether %s can be switched to \"passive\"", path);
	SAFE_FILE_PRINTF(path, "passive");
	SAFE_FILE_SCANF(path, "%s", contents);

	if (!!strncmp("passive", contents, STRING_LEN)) {
		tst_res(TFAIL, "%s is not \"passive\", but %s", path, contents);
		status = 0;
	}

	tst_res(TDEBUG, "Checking whether %s can be switched back to \"active\"", path);
	SAFE_FILE_PRINTF(path, "active");
	SAFE_FILE_SCANF(path, "%s", contents);

	if (!!strncmp("active", contents, STRING_LEN)) {
		tst_res(TFAIL, "%s is not \"active\", but %s", path, contents);
		status = 0;
	}

	snprintf(path, NAME_MAX, "/sys/devices/system/cpu/intel_pstate/no_turbo");
	tst_res(TDEBUG, "Checking whether %s can be switched to 1", path);
	SAFE_FILE_PRINTF(path, "1");
	SAFE_FILE_SCANF(path, "%s", contents);

	if (!!strncmp("1", contents, STRING_LEN)) {
		tst_res(TFAIL, "%s is not \"1\", but %s", path, contents);
		status = 0;
	}

	tst_res(TDEBUG, "Checking whether %s can be switched back to 0", path);
	SAFE_FILE_PRINTF(path, "0");
	SAFE_FILE_SCANF(path, "%s", contents);

	if (!!strncmp("0", contents, STRING_LEN)) {
		tst_res(TFAIL, "%s is not \"0\", but %s", path, contents);
		status = 0;
	}

	tst_res(TDEBUG, "Checking whether scaling_available_governors changes to \"performance schedutil \" after switching /sys/devices/system/cpu/intel_pstate/status to \"passive\"");
	snprintf(path, NAME_MAX, "/sys/devices/system/cpu/intel_pstate/status");
	SAFE_FILE_PRINTF(path, "passive");

	for (int i = 0; i < nproc; i++) {
		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_available_governors", i);
		fd = SAFE_OPEN(path, O_RDONLY);

		if (SAFE_READ(0, fd, contents, STRING_LEN) != -1) {
			if (!!strncmp("performance schedutil \n", contents, STRING_LEN)) {
				tst_res(TINFO, "%s contains: %s", path, contents);
				tst_res(TFAIL, "%s is not \"performance schedutil \"", path);
				status = 0;
			}
		}
		SAFE_CLOSE(fd);

		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
		tst_res(TDEBUG, "Checking whether %s can be changed to performance", path);
		SAFE_FILE_PRINTF(path, "performance");
		SAFE_FILE_SCANF(path, "%s", contents);

		if (!!strncmp("performance", contents, STRING_LEN)) {
			tst_res(TFAIL, "%s is not \"performance\", but %s", path, contents);
			status = 0;
		}

		tst_res(TDEBUG, "Checking whether %s can be changed to schedutil", path);
		SAFE_FILE_PRINTF(path, "schedutil");
		SAFE_FILE_SCANF(path, "%s", contents);

		if (!!strncmp("schedutil", contents, STRING_LEN)) {
			tst_res(TFAIL, "%s is not \"schedutil\", but %s", path, contents);
			status = 0;
		}
	}

	for (int i = 0; i < nproc; i++) {
		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);
		tst_res(TDEBUG, "Checking whether %s can be assigned to scaling_max_freq and scaling_min_freq", path);
		SAFE_FILE_SCANF(path, "%d", &cpuinfo_max_freq);

		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_min_freq", i);
		SAFE_FILE_SCANF(path, "%d", &cpuinfo_min_freq);

		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
		SAFE_FILE_SCANF(path, "%d", &previous_scaling_max_freq);
		SAFE_FILE_PRINTF(path, "%d", cpuinfo_max_freq);
		SAFE_FILE_SCANF(path, "%d", &scaling_max_freq);

		if (cpuinfo_max_freq < scaling_max_freq) {
			tst_res(TINFO, "cpuinfo_max_freq: %d", cpuinfo_max_freq);
			tst_res(TINFO, "scaling_max_freq: %d", scaling_max_freq);
			tst_res(TFAIL, "Failure setting %s", path);
			status = 0;
		}

		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq", i);
		SAFE_FILE_SCANF(path, "%d", &previous_scaling_min_freq);
		SAFE_FILE_PRINTF(path, "%d", cpuinfo_max_freq);
		SAFE_FILE_SCANF(path, "%d", &scaling_min_freq);

		if (cpuinfo_max_freq < scaling_min_freq) {
			tst_res(TINFO, "cpuinfo_max_freq: %d", cpuinfo_max_freq);
			tst_res(TINFO, "scaling_min_freq: %d", scaling_min_freq);
			tst_res(TFAIL, "Failure setting %s", path);
			status = 0;
		}

		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq", i);
		SAFE_FILE_PRINTF(path, "%d", previous_scaling_min_freq);

		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
		SAFE_FILE_PRINTF(path, "%d", previous_scaling_max_freq);
	}

	for (int i = 0; i < nproc; i++) {
		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);
		tst_res(TDEBUG, "Checking whether %s can be assigned to scaling_max_freq and scaling_min_freq", path);
		SAFE_FILE_SCANF(path, "%d", &cpuinfo_max_freq);

		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_min_freq", i);
		SAFE_FILE_SCANF(path, "%d", &cpuinfo_min_freq);

		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq", i);
		SAFE_FILE_SCANF(path, "%d", &previous_scaling_min_freq);
		SAFE_FILE_PRINTF(path, "%d", cpuinfo_min_freq);
		SAFE_FILE_SCANF(path, "%d", &scaling_min_freq);

		if (cpuinfo_min_freq > scaling_min_freq) {
			tst_res(TINFO, "cpuinfo_min_freq: %d", cpuinfo_min_freq);
			tst_res(TINFO, "scaling_min_freq: %d", scaling_min_freq);
			tst_res(TFAIL, "Failure setting %s", path);
			status = 0;
		}

		snprintf(path, NAME_MAX, "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
		SAFE_FILE_SCANF(path, "%d", &previous_scaling_max_freq);
		SAFE_FILE_PRINTF(path, "%d", cpuinfo_min_freq);
		SAFE_FILE_SCANF(path, "%d", &scaling_max_freq);

		if (cpuinfo_min_freq > scaling_max_freq) {
			tst_res(TINFO, "cpuinfo_min_freq: %d", cpuinfo_min_freq);
			tst_res(TINFO, "scaling_max_freq: %d", scaling_max_freq);
			tst_res(TFAIL, "Failure setting %s", path);
			status = 0;
		}
	}

	if (status)
		tst_res(TPASS, "Test passed");
}

static struct tst_test test = {
	.cleanup = cleanup,
	.needs_root = 1,
	.setup = setup,
	.supported_archs = (const char *const []) {
		"x86",
		"x86_64",
		NULL
	},
	.test_all = run
};
