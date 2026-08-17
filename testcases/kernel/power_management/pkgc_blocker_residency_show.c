// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2026 Piotr Kubaj <piotr.kubaj@intel.com>
 */

/*\
 * Verify that pkgc_blocker_residency_show reports well-formed package C-state
 * blocker residency counters that advance at a plausible rate.
 *
 * The intel_pmc_core driver prints one line per counter as "%-30s %-30u",
 * where the name is a PKGC_BLOCK_RESIDENCY_<SOURCE> token and the value counts
 * the 10us intervals during which <SOURCE> blocked a package C-state entry.
 *
 * The counter set is platform specific, so instead of checking a hardcoded
 * list the test samples the file twice and verifies that
 *
 * - every line is a PKGC_BLOCK_RESIDENCY_<SOURCE> token followed by exactly
 *   one unsigned value that fits in u32,
 * - the set of counters does not change between the two reads,
 * - no counter advances by more than the time it had to advance over allows.
 *
 * The last check is the one that catches real regressions. The values are u32
 * counting 10us intervals, so a busy source wraps in well under a day and a
 * wrap is indistinguishable from a bogus read unless the delta is evaluated
 * modulo 2^32 and bounded by the time the counter had to advance over. A
 * telemetry region read at the wrong offset then shows up as an implausible
 * jump.
 *
 * The debugfs file is only created for a PMC that exposes a package C-state
 * telemetry endpoint (pc_guid in the driver's pmc_dev_info), so on platforms
 * without one the test is not applicable.
 *
 * The test needs root because /sys/kernel/debug is mode 0700 and owned by
 * root, so the counter file cannot be opened otherwise, and because debugfs is
 * mounted if it is not mounted already.
 */

#include <sys/mount.h>
#include <time.h>
#include "tst_clocks.h"
#include "tst_fs.h"
#include "tst_safe_stdio.h"
#include "tst_test.h"
#include "tst_timer.h"

#define DEBUGFS "/sys/kernel/debug"
#define PATH DEBUGFS "/pmc_core/pkgc_blocker_residency_show"
#define PREFIX "PKGC_BLOCK_RESIDENCY_"

/* Nova Lake reports 28 counters, leave room for future platforms */
#define MAX_COUNTERS 128
#define NAME_LEN 64

/* Each count stands for a 10us interval spent blocking package C-state entry */
#define COUNTER_PERIOD_US 10

/*
 * Cap, in seconds, on how long to poll for the second sample. The PMT
 * telemetry region behind these counters refreshes at roughly 1s granularity
 * on Nova Lake, so the retries end within about a second. Two seconds is
 * enough headroom that a sample which observed no refresh at all means "every
 * source idle" rather than "polled too early".
 */
#define MAX_SAMPLE_DELAY 2

/*
 * Allowance for how stale the first sample can be. The PMC refreshes the
 * telemetry region asynchronously, so the first read can land just before a
 * refresh and return a snapshot that is almost a whole refresh period old. The
 * retry then observes a full period worth of blocking after only microseconds
 * of measured time, which the measured interval on its own does not account
 * for, and the counters that were busy over that period look implausible.
 *
 * The allowance is the same window the test is willing to poll a refresh out
 * of, so a platform refreshing up to twice as slowly as Nova Lake is covered
 * as well. It costs the check nothing in practice: a telemetry region read at
 * the wrong offset returns an arbitrary u32, which is orders of magnitude past
 * the bound either way.
 */
#define REFRESH_AGE_US (MAX_SAMPLE_DELAY * 1000000ULL)

/*
 * Slack over the theoretical maximum advance. The bound scales with the
 * interval the counter had to advance over, so what it really limits is the
 * fraction of that interval a source spent blocking, and a busy source can
 * legitimately run close to the ceiling: PKGC_BLOCK_RESIDENCY_PMC_LTR was
 * observed at 76% of it on Nova Lake. A factor of two keeps that clear of the
 * limit at any interval length, while still rejecting a bogus telemetry read,
 * which overshoots by orders of magnitude.
 */
#define SLACK 2

struct snapshot {
	char names[MAX_COUNTERS][NAME_LEN];
	uint32_t values[MAX_COUNTERS];
	int cnt;
};

static struct snapshot first, second;
static bool mounted_debugfs;

/*
 * Largest advance a counter can plausibly show over two samples taken
 * @elapsed_us microseconds apart.
 *
 * Each count stands for COUNTER_PERIOD_US of blocked time, so the interval the
 * counter had to advance over bounds how far it can move. That interval is the
 * measured one widened by REFRESH_AGE_US, because the sampled values come from
 * a telemetry snapshot that is already up to a refresh period old, not from the
 * instants the file was read.
 */
static unsigned long long max_plausible_delta(unsigned long long elapsed_us)
{
	return (elapsed_us + REFRESH_AGE_US) / COUNTER_PERIOD_US * SLACK;
}

static void read_snapshot(struct snapshot *snap)
{
	char line[256];
	FILE *fp;

	snap->cnt = 0;
	fp = SAFE_FOPEN(PATH, "r");

	while (fgets(line, sizeof(line), fp)) {
		char name[NAME_LEN], value[NAME_LEN], extra;
		unsigned long long parsed;

		line[strcspn(line, "\n")] = '\0';

		/*
		 * The value is scanned as a string rather than with %u so that
		 * a signed or out of range value is rejected instead of being
		 * quietly converted. The trailing %c rejects a third token; the
		 * space in front of it skips the padding the driver emits.
		 */
		if (sscanf(line, "%63s %63s %c", name, value, &extra) != 2) {
			tst_res(TFAIL, "malformed counter line: '%s'", line);
			continue;
		}

		if (strncmp(name, PREFIX, sizeof(PREFIX) - 1) ||
		    !name[sizeof(PREFIX) - 1]) {
			tst_res(TFAIL, "counter '%s' is not a " PREFIX "<SOURCE> token",
				name);
			continue;
		}

		if (value[strspn(value, "0123456789")]) {
			tst_res(TFAIL, "counter '%s' has a non-numeric value '%s'",
				name, value);
			continue;
		}

		parsed = strtoull(value, NULL, 10);

		if (parsed > UINT32_MAX) {
			tst_res(TFAIL, "counter '%s' value '%s' does not fit in u32",
				name, value);
			continue;
		}

		if (snap->cnt == MAX_COUNTERS)
			tst_brk(TBROK, "more than %d counters reported", MAX_COUNTERS);

		strcpy(snap->names[snap->cnt], name);
		snap->values[snap->cnt] = parsed;
		snap->cnt++;
	}

	SAFE_FCLOSE(fp);
}

static void setup(void)
{
	struct statfs sfs;

	/*
	 * DEBUGFS exists as a sysfs mount point whenever CONFIG_DEBUG_FS is
	 * set, whether or not debugfs is mounted on it, so the filesystem type
	 * has to be checked rather than the directory being present. Otherwise
	 * a platform that does export the counters looks unsupported.
	 */
	if (statfs(DEBUGFS, &sfs))
		tst_brk(TCONF | TERRNO, "cannot statfs " DEBUGFS);

	if (sfs.f_type != TST_DEBUGFS_MAGIC) {
		if (mount("debugfs", DEBUGFS, "debugfs", 0, NULL))
			tst_brk(TCONF | TERRNO, "cannot mount debugfs at " DEBUGFS);

		tst_res(TINFO, "mounted debugfs at " DEBUGFS);
		mounted_debugfs = true;
	}

	if (access(PATH, R_OK))
		tst_brk(TCONF | TERRNO, "%s not available", PATH);
}

static void cleanup(void)
{
	if (mounted_debugfs)
		SAFE_UMOUNT(DEBUGFS);
}

/*
 * Take the second sample, reporting whether it differs from the first one.
 *
 * The counters are backed by a PMT telemetry region that the PMC refreshes
 * asynchronously, so the second sample is retried until it observes a refresh
 * instead of being taken after a fixed delay. A differing counter count also
 * counts as a difference, so that the mismatch is reported rather than polled
 * over.
 */
static int second_sample_differs(void)
{
	read_snapshot(&second);

	if (first.cnt != second.cnt)
		return 1;

	for (int i = 0; i < first.cnt; i++) {
		if (first.values[i] != second.values[i])
			return 1;
	}

	return 0;
}

static void run(void)
{
	struct timespec start, end;
	unsigned long long elapsed_us, max_delta;
	int differs, fails = 0;

	tst_clock_gettime(CLOCK_MONOTONIC, &start);
	read_snapshot(&first);

	if (!first.cnt)
		tst_brk(TFAIL, "no residency counters reported");

	differs = TST_RETRY_FN_EXP_BACKOFF(second_sample_differs(),
					   TST_RETVAL_NOTNULL, MAX_SAMPLE_DELAY);

	tst_clock_gettime(CLOCK_MONOTONIC, &end);

	elapsed_us = tst_timespec_diff_us(end, start);
	max_delta = max_plausible_delta(elapsed_us);

	if (!differs) {
		tst_res(TINFO, "no counter changed over %lluus, all sources idle",
			elapsed_us);
	}

	if (first.cnt != second.cnt) {
		tst_brk(TFAIL, "counter count changed between reads: %d -> %d",
			first.cnt, second.cnt);
	}

	for (int i = 0; i < first.cnt; i++) {
		uint32_t delta;

		if (strcmp(first.names[i], second.names[i])) {
			tst_res(TFAIL, "counter %d renamed between reads: %s -> %s",
				i, first.names[i], second.names[i]);
			fails++;
			continue;
		}

		tst_res(TDEBUG, "%s: %u -> %u", first.names[i],
			first.values[i], second.values[i]);

		/*
		 * The values are u32 and wrap, so the advance has to be
		 * computed with modular arithmetic rather than by comparing the
		 * second value against the first one. A delta of 0 is normal:
		 * it means that source did not block a package C-state entry
		 * over the interval, and most counters on an idle system read 0
		 * permanently.
		 */
		delta = second.values[i] - first.values[i];

		if (delta > max_delta) {
			tst_res(TFAIL,
				"%s advanced by %u over %lluus, at most %llu plausible: %u -> %u",
				first.names[i], delta, elapsed_us, max_delta,
				first.values[i], second.values[i]);
			fails++;
		}
	}

	if (!fails) {
		tst_res(TPASS, "%d residency counters well-formed, advancing plausibly",
			first.cnt);
	}
}

static struct tst_test test = {
	.min_kver = "7.2",
	.needs_cpu_vendor = "GenuineIntel",
	.needs_root = 1,
	.needs_kconfigs = (const char *const []) {
		"CONFIG_DEBUG_FS",
		"CONFIG_INTEL_PMC_CORE",
		NULL
	},
	.supported_archs = (const char *const []) {
		"x86",
		"x86_64",
		NULL
	},
	.tags = (const struct tst_tag[]) {
		{"linux-git", "d727eb1c3ede7c21f885ded1f1ad65b47434a9b9"},
		{}
	},
	.setup = setup,
	.cleanup = cleanup,
	.test_all = run
};
