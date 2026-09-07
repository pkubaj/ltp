// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2026 Intel - http://www.intel.com/
 */

/*\
 * Verify that the package C-state LTR blocker counters only advance while a
 * package C-state entry can be attempted.
 *
 * The pkgc_ltr_blocker_show debugfs file reports, per source, how many times an
 * LTR posted by that source blocked a package C-state entry. Such an entry is
 * only attempted once every CPU in the package has gone idle, so a workload
 * that keeps every CPU busy has to stop the counters. A counter that keeps
 * advancing under full load is not counting blocked package C-state entries,
 * which is what a telemetry region read at the wrong offset looks like.
 *
 * The test first lets the system idle until it sees a counter advance, so that
 * a counter stuck at a constant value cannot pass the load phase for free. It
 * then pins a busy loop to every CPU and requires the counters to hold still.
 *
 * The counters are backed by a PMT telemetry region that the PMC refreshes
 * asynchronously and in bursts - roughly six seconds apart on Nova Lake - so
 * the load phase waits for the counters to go quiet rather than sampling them
 * after a fixed delay. Entries blocked before the load took hold surface in a
 * later refresh, while a counter that is not tracking blocked entries never
 * goes quiet at all and runs the window out.
 *
 * The test needs root because /sys/kernel/debug is mode 0700 and owned by
 * root, so the counter file cannot be opened otherwise, and because the test
 * library mounts debugfs if it is not mounted already.
 */

#define _GNU_SOURCE

#include <sched.h>
#include <signal.h>
#include <time.h>
#include "tst_clocks.h"
#include "tst_safe_stdio.h"
#include "tst_test.h"
#include "tst_timer.h"

#define PATH TST_DEBUGFS_PATH "/pmc_core/pkgc_ltr_blocker_show"

/* Nova Lake reports 5 counters, leave room for future platforms */
#define MAX_COUNTERS 64
#define NAME_LEN 64

#define POLL_MS 500

/*
 * Seconds to wait for a counter to advance while the system idles. The bursts
 * are about six seconds apart on Nova Lake, so this leaves room for two of
 * them before giving up on the platform blocking a package C-state entry at
 * all.
 */
#define IDLE_WINDOW 15

/*
 * How long, in seconds, the counters have to hold still under load, and the cap
 * on how long to wait for that. One burst period of silence is what tells a
 * stopped counter from one that is merely between refreshes, and the cap has to
 * be long enough on top of that for a burst counted before the load took hold
 * to arrive and drain.
 */
#define QUIET_WINDOW 6
#define LOAD_WINDOW 20

static char names[MAX_COUNTERS][NAME_LEN];
static int cnt;
static pid_t *spinners;
static int nr_spinners;
static volatile unsigned long long burn;

/*
 * Read the counter values, checking that the counter set itself has not changed
 * since setup(). Only the values are of interest here - the format of the file
 * is what the pkgc_ltr_blocker_show test covers - so anything unexpected about
 * the lines makes this test unable to run rather than failing it.
 */
static void read_counters(uint32_t *values)
{
	char line[256];
	FILE *fp;
	int n = 0;

	fp = SAFE_FOPEN(PATH, "r");

	while (fgets(line, sizeof(line), fp)) {
		char name[NAME_LEN];
		unsigned int value;

		line[strcspn(line, "\n")] = '\0';

		if (n == MAX_COUNTERS)
			tst_brk(TBROK, "more than %d counters reported", MAX_COUNTERS);

		if (sscanf(line, "%63s %u", name, &value) != 2)
			tst_brk(TBROK, "unexpected line format: '%s'", line);

		values[n] = value;

		if (cnt && strcmp(name, names[n])) {
			tst_brk(TBROK, "counter %d renamed during the test: %s -> %s",
				n, names[n], name);
		}

		if (!cnt)
			strcpy(names[n], name);

		n++;
	}

	/*
	 * A telemetry read that fails in the driver aborts the seq_file read
	 * instead of ending it at EOF, so without this the counters lost to the
	 * error would look like counters that stopped advancing.
	 */
	if (ferror(fp))
		tst_brk(TBROK, "reading " PATH " failed after %d counters", n);

	SAFE_FCLOSE(fp);

	if (cnt && n != cnt)
		tst_brk(TBROK, "counter count changed during the test: %d -> %d", cnt, n);

	cnt = n;
}

static void spin(void)
{
	/* volatile so that the loop survives optimization */
	for (;;)
		burn++;
}

/*
 * Keep every CPU the test may run on busy, so that the package cannot go idle
 * and no package C-state entry can be attempted.
 *
 * The spinners are pinned one per CPU taken from the test's own affinity mask,
 * rather than left to the load balancer, so that a CPU is not left idle by a
 * scheduling decision. Offline CPUs and a restricted mask are handled by
 * construction.
 */
static void start_load(void)
{
	cpu_set_t mask;
	int cpu = 0;

	if (sched_getaffinity(0, sizeof(mask), &mask))
		tst_brk(TBROK | TERRNO, "sched_getaffinity() failed");

	nr_spinners = CPU_COUNT(&mask);
	spinners = SAFE_MALLOC(nr_spinners * sizeof(*spinners));

	for (int i = 0; i < nr_spinners; i++) {
		pid_t pid;

		while (!CPU_ISSET(cpu, &mask))
			cpu++;

		pid = SAFE_FORK();

		if (!pid) {
			cpu_set_t one;

			CPU_ZERO(&one);
			CPU_SET(cpu, &one);

			if (sched_setaffinity(0, sizeof(one), &one)) {
				tst_brk(TBROK | TERRNO,
					"sched_setaffinity(CPU %d) failed", cpu);
			}

			spin();
		}

		spinners[i] = pid;
		cpu++;
	}

	tst_res(TINFO, "keeping %d CPUs busy", nr_spinners);
}

static void stop_load(void)
{
	if (!spinners)
		return;

	for (int i = 0; i < nr_spinners; i++) {
		SAFE_KILL(spinners[i], SIGKILL);
		SAFE_WAITPID(spinners[i], NULL, 0);
	}

	free(spinners);
	spinners = NULL;
}

/*
 * Wait for a counter to advance while the system idles, reporting how many did.
 *
 * This is what tells a counter that stops under load from one that never moves.
 * On an idle system most counters read 0 permanently, because nothing posts an
 * LTR that would block a package C-state entry, so a single counter advancing
 * is enough.
 */
static int wait_for_advance(const uint32_t *baseline)
{
	uint32_t values[MAX_COUNTERS];
	struct timespec start, now;
	int advanced = 0;

	tst_clock_gettime(CLOCK_MONOTONIC, &start);

	for (;;) {
		read_counters(values);

		for (int i = 0; i < cnt; i++) {
			if (values[i] == baseline[i])
				continue;

			tst_res(TINFO, "%s advanced by %u while idle", names[i],
				values[i] - baseline[i]);
			advanced++;
		}

		if (advanced)
			return advanced;

		tst_clock_gettime(CLOCK_MONOTONIC, &now);

		if (tst_timespec_diff_ms(now, start) >= IDLE_WINDOW * 1000)
			return 0;

		usleep(POLL_MS * 1000);
	}
}

/*
 * Require the counters to hold still for QUIET_WINDOW seconds of full load.
 */
static void check_counters_stop(void)
{
	uint32_t first[MAX_COUNTERS], prev[MAX_COUNTERS], values[MAX_COUNTERS];
	bool moved[MAX_COUNTERS] = {};
	struct timespec start, last_change, now;

	read_counters(first);
	memcpy(prev, first, sizeof(prev));

	tst_clock_gettime(CLOCK_MONOTONIC, &start);
	last_change = start;

	for (;;) {
		usleep(POLL_MS * 1000);

		read_counters(values);
		tst_clock_gettime(CLOCK_MONOTONIC, &now);

		for (int i = 0; i < cnt; i++) {
			if (values[i] == prev[i])
				continue;

			moved[i] = true;
			last_change = now;
		}

		memcpy(prev, values, sizeof(prev));

		if (tst_timespec_diff_ms(now, last_change) >= QUIET_WINDOW * 1000) {
			tst_res(TPASS,
				"%d counters held still over %ds with every CPU busy",
				cnt, QUIET_WINDOW);
			return;
		}

		if (tst_timespec_diff_ms(now, start) < LOAD_WINDOW * 1000)
			continue;

		for (int i = 0; i < cnt; i++) {
			if (!moved[i])
				continue;

			tst_res(TFAIL,
				"%s kept advancing over %ds with every CPU busy: %u -> %u",
				names[i], LOAD_WINDOW, first[i], values[i]);
		}

		return;
	}
}

static void setup(void)
{
	uint32_t values[MAX_COUNTERS];

	if (access(PATH, R_OK))
		tst_brk(TCONF | TERRNO, "%s not available", PATH);

	/* Learns the counter set the rest of the test compares against */
	read_counters(values);

	if (!cnt)
		tst_brk(TBROK, "no LTR blocker counters reported");
}

static void cleanup(void)
{
	stop_load();
}

static void run(void)
{
	uint32_t baseline[MAX_COUNTERS];

	read_counters(baseline);

	/*
	 * Without a counter that moves while idle there is nothing to stop
	 * under load, a stuck counter would look exactly the same.
	 */
	if (!wait_for_advance(baseline)) {
		tst_brk(TCONF,
			"no LTR blocked a package C-state entry over %ds of idling",
			IDLE_WINDOW);
	}

	start_load();
	check_counters_stop();
	stop_load();
}

static struct tst_test test = {
	.min_kver = "7.2",
	.timeout = 90,
	.needs_root = 1,
	.needs_debugfs = 1,
	.forks_child = 1,
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
	.cleanup = cleanup,
	.test_all = run
};
