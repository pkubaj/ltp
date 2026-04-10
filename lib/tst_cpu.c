/*
 *   Copyright (c) 2012 Fujitsu Ltd.
 *   Author: Wanlong Gao <gaowanlong@cn.fujitsu.com>
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "lapi/cpuset.h"

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#define TST_NO_DEFAULT_MAIN
#include "tst_test.h"
#include "tst_safe_stdio.h"

long tst_ncpus(void)
{
	long ncpus = -1;
#ifdef _SC_NPROCESSORS_ONLN
	ncpus = SAFE_SYSCONF(_SC_NPROCESSORS_ONLN);
#else
	tst_brk(TBROK, "could not determine number of CPUs online");
#endif
	return ncpus;
}

long tst_ncpus_conf(void)
{
	long ncpus_conf = -1;
#ifdef _SC_NPROCESSORS_CONF
	ncpus_conf = SAFE_SYSCONF(_SC_NPROCESSORS_CONF);
#else
	tst_brk(TBROK, "could not determine number of CPUs configured");
#endif
	return ncpus_conf;
}

#define KERNEL_MAX "/sys/devices/system/cpu/kernel_max"

long tst_ncpus_max(void)
{
	long ncpus_max = -1;
	struct stat buf;

	/* sched_getaffinity() and sched_setaffinity() cares about number of
	 * possible CPUs the OS or hardware can support, which can be larger
	 * than what sysconf(_SC_NPROCESSORS_CONF) currently provides
	 * (by enumarating /sys/devices/system/cpu/cpu* entries).
	 *
	 *  Use /sys/devices/system/cpu/kernel_max, if available. This
	 *  represents NR_CPUS-1, a compile time option which specifies
	 *  "maximum number of CPUs which this kernel will support".
	 *  This should provide cpu mask size large enough for any purposes. */
	if (stat(KERNEL_MAX, &buf) == 0) {
		SAFE_FILE_SCANF(KERNEL_MAX, "%ld", &ncpus_max);
		/* this is maximum CPU index allowed by the kernel
		 * configuration, so # of cpus allowed by config is +1 */
		ncpus_max++;
	} else {
		/* fall back to _SC_NPROCESSORS_CONF */
		ncpus_max = tst_ncpus_conf();
	}
	return ncpus_max;
}

long tst_ncpus_available(void)
{
#ifdef CPU_COUNT_S
	long ncpus = tst_ncpus_max();
	size_t cpusz = CPU_ALLOC_SIZE(ncpus);
	cpu_set_t *cpus = CPU_ALLOC(ncpus);

	if (!cpus)
		tst_brk(TBROK | TERRNO, "CPU_ALLOC(%zu)", cpusz);

	if (sched_getaffinity(0, cpusz, cpus)) {
		tst_res(TWARN | TERRNO, "sched_getaffinity(0, %zu, %zx)",
			cpusz, (size_t)cpus);
	} else {
		ncpus = CPU_COUNT_S(cpusz, cpus);
	}
	CPU_FREE(cpus);

	return ncpus;
#else
	return tst_ncpus();
#endif
}

#define CPUINFO_FILE "/proc/cpuinfo"
#define MAX_LINE 1024

char *tst_get_cpuinfo(int cpu, char *item)
{
#ifdef __x86_64__
	char line[MAX_LINE];
	char *item_value = tst_alloc(MAX_LINE);
	char *_value;
	bool cpu_found = false;
	FILE *f = SAFE_FOPEN(CPUINFO_FILE, "r");
	int _cpu;
	int size_s;

	while (fgets(line, sizeof(line), f)) {
		if (cpu_found) {
			if (strstr(line, item)) {
				_value = strchr(line, ':');
				if (_value) {
				    size_s = strlen(_value);
					// delete two first chars ': '
					strncpy(item_value, _value + 2, size_s);
					item_value[strcspn(item_value, "\n")] = '\0';
					break;
				};
			};
		} else if (strstr(line, "processor")) {
			SAFE_SSCANF(line, "processor : %d", &_cpu);
			if (cpu == _cpu)
				cpu_found = true;
		}
	};
	SAFE_FCLOSE(f);
	return item_value;
#else /* __x86_64__ */
	tst_brk(TBROK | TERRNO, "tst_get_cpuinfo - supports only x86_64 architecture.");
#endif /* __x86_64__ */
};
