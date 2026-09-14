// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2026 Piotr Kubaj <piotr.kubaj@intel.com>
 */

/*\
 * Verify that pkgc_ltr_blocker_show reports well-formed package C-state LTR
 * blocker counters that advance at a plausible rate.
 *
 * The intel_pmc_core driver prints one line per counter as "%-30s %-30u",
 * where the name is a PKGC_PREVENT_LTR_<SOURCE> token and the value counts how
 * many times an LTR posted by <SOURCE> blocked a package C-state entry.
 *
 * The counter set is platform specific, so instead of checking a hardcoded
 * list the test samples the file twice and verifies that
 *
 * - the file can be read in full, so that a failing telemetry read in the
 *   driver is not mistaken for a short counter list,
 * - every line is a PKGC_PREVENT_LTR_<SOURCE> token followed by exactly one
 *   unsigned value that fits in u32,
 * - the set of counters does not change between the two reads,
 * - no counter advances by more entries than the package could have attempted
 *   over the interval.
 *
 * The last check is the one that catches a telemetry region read at the wrong
 * offset. The values are u32 and wrap, so the advance has to be evaluated
 * modulo 2^32, and a wrap is then indistinguishable from a bogus read unless
 * the delta is bounded by the time the counter had to advance over. An
 * arbitrary u32 read out of the wrong offset averages 2^31, orders of
 * magnitude past what the bound allows.
 *
 * The debugfs file is only created for a PMC that exposes a package C-state
 * telemetry endpoint (pc_guid in the driver's pmc_dev_info), so on platforms
 * without one the test is not applicable.
 *
 * The test needs root because /sys/kernel/debug is mode 0700 and owned by
 * root, so the counter file cannot be opened otherwise, and because the test
 * library mounts debugfs if it is not mounted already.
 */

#include <time.h>
#include "tst_clocks.h"
#include "tst_safe_stdio.h"
#include "tst_test.h"
#include "tst_timer.h"

#define PATH TST_DEBUGFS_PATH "/pmc_core/pkgc_ltr_blocker_show"
#define PREFIX "PKGC_PREVENT_LTR_"

/* Nova Lake reports 5 counters, leave room for future platforms */
#define MAX_COUNTERS 64
#define NAME_LEN 64

/*
 * Cap, in seconds, on how long to poll for a second sample that differs from
 * the first one. On an idle Nova Lake the counters advance in bursts a few
 * seconds apart, because a blocked entry needs the package to attempt a C-state
 * entry in the first place, so a window of a few seconds is what it takes for
 * the delta check to have anything to look at. TST_RETRY_FN_EXP_BACKOFF() ends
 * up sleeping about twice the cap in total, hence the modest value here.
 */
#define MAX_SAMPLE_DELAY 4

/*
 * Allowance for how stale the first sample can be. The PMC refreshes the
 * telemetry region behind these counters asynchronously, so the first read can
 * land just before a refresh and return a snapshot that is almost a whole
 * refresh period old. The retry then observes a period worth of blocked entries
 * after only microseconds of measured time, which the measured interval on its
 * own does not account for.
 *
 * The allowance is the same window the test is willing to poll a refresh out
 * of, which on Nova Lake is several times the observed refresh period.
 */
#define REFRESH_AGE_US (MAX_SAMPLE_DELAY * 1000000ULL)

/*
 * Blocked entries per microsecond the bound allows. Each count stands for one
 * package C-state entry that was blocked, and entries can only be attempted as
 * fast as the package can cycle between idle and running, which takes
 * microseconds. Ten per microsecond is therefore already past what the hardware
 * can produce - the counters were observed advancing by about 6 per second on
 * an idle Nova Lake - and the headroom costs the check nothing, because the
 * bogus read it exists to catch overshoots it by orders of magnitude anyway.
 */
#define MAX_BLOCKS_PER_US 10

struct snapshot {
	char names[MAX_COUNTERS][NAME_LEN];
	uint32_t values[MAX_COUNTERS];
	int cnt;
};

static struct snapshot first, second;

/*
 * Largest advance a counter can plausibly show over two samples taken
 * @elapsed_us microseconds apart.
 *
 * The interval the counter had to advance over is the measured one widened by
 * REFRESH_AGE_US, because the sampled values come from a telemetry snapshot
 * that is already up to a refresh period old, not from the instants the file
 * was read.
 */
static unsigned long long max_plausible_delta(unsigned long long elapsed_us)
{
	return (elapsed_us + REFRESH_AGE_US) * MAX_BLOCKS_PER_US;
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

	/*
	 * A telemetry read that fails in the driver aborts the seq_file read
	 * instead of ending it at EOF, so without this the counters lost to the
	 * error would look like a platform reporting fewer of them.
	 */
	if (ferror(fp))
		tst_brk(TFAIL, "reading " PATH " failed after %d counters", snap->cnt);

	SAFE_FCLOSE(fp);
}

static void setup(void)
{
	if (access(PATH, R_OK))
		tst_brk(TCONF | TERRNO, "%s not available", PATH);
}

/*
 * Take the second sample, reporting whether it differs from the first one.
 *
 * A blocked entry is a sporadic event and the counters are backed by a PMT
 * telemetry region that the PMC refreshes asynchronously, so the second sample
 * is retried until it observes a change instead of being taken after a fixed
 * delay. A differing counter count also counts as a difference, so that the
 * mismatch is reported rather than polled over.
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
		tst_brk(TFAIL, "no LTR blocker counters reported");

	differs = TST_RETRY_FN_EXP_BACKOFF(second_sample_differs(),
					   TST_RETVAL_NOTNULL, MAX_SAMPLE_DELAY);

	tst_clock_gettime(CLOCK_MONOTONIC, &end);

	elapsed_us = tst_timespec_diff_us(end, start);
	max_delta = max_plausible_delta(elapsed_us);

	if (!differs) {
		tst_res(TINFO, "no counter changed over %lluus, no entry was blocked",
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
		 * it means no entry was blocked by that source over the
		 * interval, and most counters on an idle system read 0
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
		tst_res(TPASS, "%d LTR blocker counters well-formed, advancing plausibly",
			first.cnt);
	}
}

static struct tst_test test = {
	.min_kver = "7.2",
	.needs_root = 1,
	.needs_debugfs = 1,
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
		{"linux-git", "38c79dd63b72e36919ef097d4e5025ca0fa17f34"},
		{}
	},
	.setup = setup,
	.test_all = run
};
