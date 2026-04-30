// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2026 Piotr Kubaj <piotr.kubaj@intel.com>
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

static bool *online;
static char path[PATH_MAX], intel_pstate_status[16];
static int no_turbo, nproc;
static long *previous_scaling_max_freq, *previous_scaling_min_freq;

static void cleanup(void)
{
	SAFE_FILE_PRINTF("/sys/devices/system/cpu/intel_pstate/no_turbo", "%d", no_turbo);
	SAFE_FILE_PRINTF("/sys/devices/system/cpu/intel_pstate/status", "%s", intel_pstate_status);
	for (int i = 0; i < nproc; i++) {
		if (!online[i])
			continue;

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
		SAFE_FILE_PRINTF(path, "%ld", previous_scaling_max_freq[i]);

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq", i);
		SAFE_FILE_PRINTF(path, "%ld", previous_scaling_min_freq[i]);
	}
	free(online);
	free(previous_scaling_max_freq);
	free(previous_scaling_min_freq);
}

static void setup(void)
{
	SAFE_FILE_SCANF("/sys/devices/system/cpu/intel_pstate/no_turbo", "%d", &no_turbo);
	SAFE_FILE_SCANF("/sys/devices/system/cpu/intel_pstate/status", "%15s", intel_pstate_status);
	SAFE_FILE_PRINTF("/sys/devices/system/cpu/intel_pstate/status", "active");

	nproc = tst_ncpus_conf();
	online = SAFE_CALLOC(nproc, sizeof(*online));
	previous_scaling_max_freq = SAFE_CALLOC(nproc, sizeof(*previous_scaling_max_freq));
	previous_scaling_min_freq = SAFE_CALLOC(nproc, sizeof(*previous_scaling_min_freq));
	online[0] = true;
	for (int i = 1; i < nproc; i++) {
		int tmp;

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/online", i);
		SAFE_FILE_SCANF(path, "%d", &tmp);
		online[i] = (bool)tmp;
	}
	for (int i = 0; i < nproc; i++) {
		if (!online[i])
			continue;

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
		SAFE_FILE_SCANF(path, "%ld", &previous_scaling_max_freq[i]);
		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq", i);
		SAFE_FILE_SCANF(path, "%ld", &previous_scaling_min_freq[i]);
	}
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
	char contents[256] = {0}, path_cpuinfo_min_freq[PATH_MAX];
	int fd;
	long cpuinfo_max_freq = 0, cpuinfo_min_freq = 0, scaling_max_freq, scaling_min_freq;

	for (int i = 0; i < nproc; i++) {
		if (!online[i])
			continue;

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_driver", i);

		SAFE_FILE_SCANF(path, "%255s", contents);
		tst_res(TDEBUG, "Checking whether %s is \"intel_pstate\"", path);
		if (strstr(contents, "intel_pstate"))
			tst_res(TPASS, "%s is intel_pstate", path);
		else {
			tst_res(TINFO, "%s contains: %s", path, contents);
			tst_res(TFAIL, "%s is not intel_pstate", path);
		}

		for (int j = 0; cpufreq_nodes[j] != NULL; j++) {
			struct stat stats;

			snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/%s", i, cpufreq_nodes[j]);
			tst_res(TDEBUG, "Checking whether %s is a regular file", path);
			SAFE_STAT(path, &stats);
			if (!S_ISREG(stats.st_mode)) {
				tst_res(TINFO, "%s mode: %o", path, stats.st_mode);
				tst_res(TFAIL, "%s is not a regular file", path);
			} else
				tst_res(TPASS, "%s is a regular file", path);
		}

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_available_governors", i);
		memset(contents, 0, sizeof(contents));
		fd = SAFE_OPEN(path, O_RDONLY);
		SAFE_READ(0, fd, contents, sizeof(contents));
		SAFE_CLOSE(fd);

		tst_res(TDEBUG, "Checking whether %s contains \"performance\" and \"powersave\"", path);
		if (strstr(contents, "performance") && strstr(contents, "powersave"))
			tst_res(TPASS, "%s contains: %s", path, contents);
		else {
			tst_res(TINFO, "%s contains: %s", path, contents);
			tst_res(TFAIL, "%s is not \"performance powersave\"", path);
		}

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
		tst_res(TDEBUG, "Checking %s", path);
		memset(contents, 0, sizeof(contents));
		SAFE_FILE_SCANF(path, "%255s", contents);
		if (strstr(contents, "performance")) {
			tst_res(TDEBUG, "%s is \"performance\"", path);
			tst_res(TDEBUG, "Checking whether %s can be switched to \"powersave\"", path);
			SAFE_FILE_PRINTF(path, "powersave");
			memset(contents, 0, sizeof(contents));
			SAFE_FILE_SCANF(path, "%255s", contents);
			if (strstr(contents, "powersave"))
				tst_res(TPASS, "Changing scaling_governor from performance to powersave succeeded");
			else
				tst_res(TFAIL, "%s: failed to change scaling_governor from performance to powersave, current scaling_governor: %s", path, contents);

		} else if (strstr(contents, "powersave")) {
			tst_res(TDEBUG, "%s is \"powersave\"", path);
			tst_res(TDEBUG, "Checking whether %s can be switched to \"performance\"", path);
			SAFE_FILE_PRINTF(path, "performance");
			memset(contents, 0, sizeof(contents));
			SAFE_FILE_SCANF(path, "%255s", contents);
			if (strstr(contents, "performance"))
				tst_res(TPASS, "Changing scaling_governor from powersave to performance succeeded");
			else
				tst_res(TFAIL, "%s: failed to change scaling_governor from powersave to performance, current scaling_governor: %s", path, contents);

		} else
			tst_brk(TBROK, "Unknown scaling_governor: %s", contents);

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);
		snprintf(path_cpuinfo_min_freq, sizeof(path_cpuinfo_min_freq), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_min_freq", i);
		tst_res(TDEBUG, "Checking whether %s is bigger than cpuinfo_min_freq", path);
		SAFE_FILE_SCANF(path, "%ld", &cpuinfo_max_freq);

		SAFE_FILE_SCANF(path_cpuinfo_min_freq, "%ld", &cpuinfo_min_freq);

		if (cpuinfo_min_freq > cpuinfo_max_freq)
			tst_res(TFAIL, "Value in %s: %ld, should be bigger than cpuinfo_min_freq: %ld", path, cpuinfo_max_freq, cpuinfo_min_freq);
		else
			tst_res(TPASS, "Value in %s: %ld, is bigger than cpuinfo_min_freq: %ld", path, cpuinfo_max_freq, cpuinfo_min_freq);
	}

	snprintf(path, sizeof(path), "/sys/devices/system/cpu/intel_pstate/status");
	tst_res(TDEBUG, "Checking whether %s contains \"active\"", path);
	memset(contents, 0, sizeof(contents));
	SAFE_FILE_SCANF(path, "%255s", contents);

	if (strstr(contents, "active"))
		tst_res(TPASS, "%s is \"active\"", path);
	else
		tst_res(TFAIL, "%s is not \"active\", but %s", path, contents);

	tst_res(TDEBUG, "Checking whether %s can be switched to \"passive\"", path);
	SAFE_FILE_PRINTF(path, "passive");
	memset(contents, 0, sizeof(contents));
	SAFE_FILE_SCANF(path, "%255s", contents);

	if (strstr(contents, "passive"))
		tst_res(TPASS, "%s is \"passive\"", path);
	else
		tst_res(TFAIL, "%s is not \"passive\", but %s", path, contents);

	tst_res(TDEBUG, "Checking whether %s can be switched back to \"active\"", path);
	SAFE_FILE_PRINTF(path, "active");
	memset(contents, 0, sizeof(contents));
	SAFE_FILE_SCANF(path, "%255s", contents);

	if (strstr(contents, "active"))
		tst_res(TPASS, "%s is \"active\"", path);
	else
		tst_res(TFAIL, "%s is not \"active\", but %s", path, contents);

	snprintf(path, sizeof(path), "/sys/devices/system/cpu/intel_pstate/no_turbo");
	tst_res(TDEBUG, "Checking whether %s can be switched to 1", path);
	SAFE_FILE_PRINTF(path, "1");
	memset(contents, 0, sizeof(contents));
	SAFE_FILE_SCANF(path, "%255s", contents);

	if (strstr(contents, "1"))
		tst_res(TPASS, "%s is \"1\"", path);
	else
		tst_res(TFAIL, "%s is not \"1\", but %s", path, contents);

	tst_res(TDEBUG, "Checking whether %s can be switched back to 0", path);
	SAFE_FILE_PRINTF(path, "0");
	memset(contents, 0, sizeof(contents));
	SAFE_FILE_SCANF(path, "%255s", contents);

	if (strstr(contents, "0"))
		tst_res(TPASS, "%s is \"0\"", path);
	else
		tst_res(TFAIL, "%s is not \"0\", but %s", path, contents);

	tst_res(TDEBUG, "Checking whether scaling_available_governors contains \"performance\" and \"schedutil\" after switching /sys/devices/system/cpu/intel_pstate/status to \"passive\"");
	snprintf(path, sizeof(path), "/sys/devices/system/cpu/intel_pstate/status");
	SAFE_FILE_PRINTF(path, "passive");

	for (int i = 0; i < nproc; i++) {
		if (!online[i])
			continue;

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_available_governors", i);
		memset(contents, 0, sizeof(contents));
		fd = SAFE_OPEN(path, O_RDONLY);
		SAFE_READ(0, fd, contents, sizeof(contents));
		SAFE_CLOSE(fd);

		tst_res(TDEBUG, "Checking whether %s contains \"performance\" and \"schedutil\"", path);
		if (strstr(contents, "performance") && strstr(contents, "schedutil"))
			tst_res(TPASS, "%s contains: %s", path, contents);
		else {
			tst_res(TINFO, "%s contains: %s", path, contents);
			tst_res(TFAIL, "%s does not contain \"performance\" and \"schedutil\"", path);
		}

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", i);
		tst_res(TDEBUG, "Checking whether %s can be changed to performance", path);
		SAFE_FILE_PRINTF(path, "performance");
		memset(contents, 0, sizeof(contents));
		SAFE_FILE_SCANF(path, "%255s", contents);

		if (strstr(contents, "performance"))
			tst_res(TPASS, "%s is \"performance\"", path);
		else
			tst_res(TFAIL, "%s is not \"performance\", but %s", path, contents);

		tst_res(TDEBUG, "Checking whether %s can be changed to schedutil", path);
		SAFE_FILE_PRINTF(path, "schedutil");
		memset(contents, 0, sizeof(contents));
		SAFE_FILE_SCANF(path, "%255s", contents);

		if (strstr(contents, "schedutil"))
			tst_res(TPASS, "%s is \"schedutil\"", path);
		else
			tst_res(TFAIL, "%s is not \"schedutil\", but %s", path, contents);
	}

	for (int i = 0; i < nproc; i++) {
		if (!online[i])
			continue;

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);
		tst_res(TDEBUG, "Checking whether %s can be assigned to scaling_max_freq and scaling_min_freq", path);
		SAFE_FILE_SCANF(path, "%ld", &cpuinfo_max_freq);

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_min_freq", i);
		SAFE_FILE_SCANF(path, "%ld", &cpuinfo_min_freq);

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
		SAFE_FILE_PRINTF(path, "%ld", cpuinfo_max_freq);
		SAFE_FILE_SCANF(path, "%ld", &scaling_max_freq);

		if (cpuinfo_max_freq < scaling_max_freq) {
			tst_res(TINFO, "cpuinfo_max_freq: %ld", cpuinfo_max_freq);
			tst_res(TINFO, "scaling_max_freq: %ld", scaling_max_freq);
			tst_res(TFAIL, "Failure setting %s", path);
		} else
			tst_res(TPASS, "Successfully set up %s", path);

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq", i);
		SAFE_FILE_PRINTF(path, "%ld", cpuinfo_max_freq);
		SAFE_FILE_SCANF(path, "%ld", &scaling_min_freq);

		if (cpuinfo_max_freq < scaling_min_freq) {
			tst_res(TINFO, "cpuinfo_max_freq: %ld", cpuinfo_max_freq);
			tst_res(TINFO, "scaling_min_freq: %ld", scaling_min_freq);
			tst_res(TFAIL, "Failure setting %s", path);
		} else
			tst_res(TPASS, "Successfully set up %s", path);

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq", i);
		SAFE_FILE_PRINTF(path, "%ld", previous_scaling_min_freq[i]);

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
		SAFE_FILE_PRINTF(path, "%ld", previous_scaling_max_freq[i]);
	}

	for (int i = 0; i < nproc; i++) {
		if (!online[i])
			continue;

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", i);
		tst_res(TDEBUG, "Checking whether %s can be assigned to scaling_max_freq and scaling_min_freq", path);
		SAFE_FILE_SCANF(path, "%ld", &cpuinfo_max_freq);

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/cpuinfo_min_freq", i);
		SAFE_FILE_SCANF(path, "%ld", &cpuinfo_min_freq);

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_min_freq", i);
		SAFE_FILE_PRINTF(path, "%ld", cpuinfo_min_freq);
		SAFE_FILE_SCANF(path, "%ld", &scaling_min_freq);

		if (cpuinfo_min_freq > scaling_min_freq) {
			tst_res(TINFO, "cpuinfo_min_freq: %ld", cpuinfo_min_freq);
			tst_res(TINFO, "scaling_min_freq: %ld", scaling_min_freq);
			tst_res(TFAIL, "Failure setting %s", path);
		} else
			tst_res(TPASS, "Successfully set up %s", path);

		snprintf(path, sizeof(path), "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_max_freq", i);
		SAFE_FILE_PRINTF(path, "%ld", cpuinfo_min_freq);
		SAFE_FILE_SCANF(path, "%ld", &scaling_max_freq);

		if (cpuinfo_min_freq > scaling_max_freq) {
			tst_res(TINFO, "cpuinfo_min_freq: %ld", cpuinfo_min_freq);
			tst_res(TINFO, "scaling_max_freq: %ld", scaling_max_freq);
			tst_res(TFAIL, "Failure setting %s", path);
		} else
			tst_res(TPASS, "Successfully set up %s", path);
	}
}

static struct tst_test test = {
	.cleanup = cleanup,
	.needs_kconfigs = (const char *const []) {
		"CONFIG_X86_INTEL_PSTATE",
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
