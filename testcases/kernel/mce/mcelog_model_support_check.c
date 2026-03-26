// SPDX-License-Identifier: GPL-2.0-or-later

/*
 * Copyright (C) 2025-2026 Intel - http://www.intel.com/
 */

/*
 * Check mcelog support for platform CPU model number and family number.
 */

#include <sys/klog.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include "tst_module.h"
#include "tst_test.h"
#include "tst_kconfig.h"
#include "tst_safe_stdio.h"
#include "tst_safe_file_ops.h"

#define TNAME "mcelog-cpu-model-number-support"

#define MCE_INJECT_MODULE "mce_inject"
#define MCELOG_DAEMON "mcelog --daemon"
#define LTPROOT "/opt/ltp"

#define fcpuinfo "/proc/cpuinfo"
#define flsmod "/proc/modules"
#define fdmesg "/dev/kmsg"

#define TMARKER NULL
#define TMCEDIR NULL

// TODO: FIX IT: not all value is int, we can got many types
#define SAFE_READ_CPUINFO(item) \
		({int tst_rval; \
		SAFE_FILE_LINES_SCANF(fcpuinfo, item"%*s%d", \
		&tst_rval); \
		tst_rval;})

#define IS_MODULE_ENABLED(module) \
		({find_in_file(flsmod, module);})

#define IS_FOUND_IN_DMESG(search, marker) \
		({find_in_fdmesg(search, marker);})

#define KLOG_SIZE_BUFFER 10
#define KLOG_READ_ALL 3

#define DCALL_TRACE "Call Trace:"
#define DKERNEL_BUG "kernel BUG"
#define DKERNEL_PANIC "Kernel panic"
#define DSEGFAULT "segfault"
#define DSOFT_LOCKUP "soft lockup"
#define DHUNG_TASK "hung_task"
#define DOUT_OF_MEMORY "Out of memory"
#define DGEN_PROT_FAULT "general protection fault"
#define DKASAN "KASAN:"

static char *DMESG_CRITICAL_PATTERNS[] = {
	DCALL_TRACE, DKERNEL_BUG, DKERNEL_PANIC, DSEGFAULT, DSOFT_LOCKUP,
	DHUNG_TASK, DOUT_OF_MEMORY, DGEN_PROT_FAULT, DKASAN
};

static char *TMP_MCE_DIR;
static char *LOG_MCE_FILE;
static bool MCE_INJECT_MODULE_UNLOAD;

static bool find_in_file(const char *path, const char *search)
{
	char line[PATH_MAX];
	FILE *file = SAFE_FOPEN(path, "r");
	bool found = false;

	while (fgets(line, sizeof(line), file)) {
		if (strstr(line, search)) {
			found = true;
			break;
		};
	};
	SAFE_FCLOSE(file);
	return found;
};

static char *get_dmesg_from_marker(char *marker)
{
	const size_t dmesg_file_name_size = 316;
	char dmesg_file_name[dmesg_file_name_size];
	FILE *dmesg;
	const size_t buf_dmesg_size = klogctl(KLOG_SIZE_BUFFER, NULL, 0);
	char *buf_dmesg = SAFE_MALLOC(buf_dmesg_size);
	int ret = klogctl(KLOG_READ_ALL, buf_dmesg, buf_dmesg_size);
	const size_t marker_dmesg_name_size = 252;
	char *marker_dmesg_name = SAFE_MALLOC(marker_dmesg_name_size);
	FILE *marker_dmesg;
	char line[PATH_MAX];
	bool write_msg = false;

	snprintf(dmesg_file_name, dmesg_file_name_size, "%s/dmesg-all-%s.txt",
			TMP_MCE_DIR, marker);
	dmesg = SAFE_FOPEN(dmesg_file_name, "w");
	fwrite(buf_dmesg, 1, ret, dmesg);
	SAFE_FCLOSE(dmesg);
	free(buf_dmesg);

	snprintf(marker_dmesg_name, marker_dmesg_name_size, "%s/dmesg-cut-%s.txt",
			TMP_MCE_DIR, marker);
	tst_res(TDEBUG, "Get file with dmesg for marker: %s", marker_dmesg_name);

	marker_dmesg = SAFE_FOPEN(marker_dmesg_name, "w");
	dmesg = SAFE_FOPEN(dmesg_file_name, "r");
	while (fgets(line, sizeof(line), dmesg)) {
		if (strstr(line, marker))
			write_msg = true;
		if (write_msg)
			fprintf(marker_dmesg, "%s", line);
	};

	SAFE_FCLOSE(marker_dmesg);
	SAFE_FCLOSE(dmesg);

	return marker_dmesg_name;
};

static bool find_in_fdmesg(const char *search, char *marker)
{
	if (marker != NULL) {
		char *filename = get_dmesg_from_marker(marker);
		bool found = find_in_file(filename, search);

		free(filename);
		return found;
	} else
		return find_in_file(fdmesg, search);
		// TODO: if marker is not set, check dmesg from the end?
};

static void check_dmesg_errors(char *marker)
{
	bool found = false;
	int size = ARRAY_SIZE(DMESG_CRITICAL_PATTERNS);
	char *mdmesg = get_dmesg_from_marker(marker);

	tst_res(TDEBUG, "Check dmesg errors");
	for (int i = 0; i < size; i++) {
		if(find_in_file(mdmesg, DMESG_CRITICAL_PATTERNS[i]))
			found = true;
	};
	if (found)
		tst_res(TFAIL, "Found errors in dmesg.");
	else
		tst_res(TINFO, "No errors in dmesg.");
	free(mdmesg);
};

static char *generate_marker(void)
{
	const size_t marker_size = 128;
	char *marker = SAFE_MALLOC(marker_size);
	time_t t = time(NULL);
	struct timespec ts;
	const size_t now_time_size = 64;
	char now_time[now_time_size];

	clock_gettime(CLOCK_REALTIME, &ts);
	strftime(now_time, now_time_size, "%Y%m%d%H%M%S", localtime(&t));
	snprintf(marker, marker_size, "LTP-MARKER-%s-%s%03ld", TNAME, now_time,
			ts.tv_nsec / 1000000);
	tst_res(TDEBUG, "Generated new marker: %s", marker);

	return marker;
};

static void write_to_dmesg(char *msg)
{
	FILE *file = SAFE_FOPEN(fdmesg, "a");

	tst_res(TDEBUG, "Write to dmesg");
	fprintf(file, "Marker: %s\n", msg);
	SAFE_FCLOSE(file);
};

static char *get_logfile(char *marker)
{
	const size_t logfile_size = strlen(TMP_MCE_DIR) + strlen(marker) + 25;
	char *logfile = SAFE_MALLOC(logfile_size);

	snprintf(logfile, logfile_size, "%s/mcelog-logfile-%s.txt", TMP_MCE_DIR, marker);
	return logfile;
};

static bool is_module_enabled(const char *module)
{
	bool enabled;

	tst_res(TDEBUG, "Check if module is enabled");
	enabled = IS_MODULE_ENABLED(module);
	if (enabled)
		tst_res(TINFO, "module '%s' is enabled", module);
	else
		tst_res(TDEBUG, "not found module '%s'", module);
	return enabled;
};

static void check_module_initialization(const char *module, char *marker)
{
	const size_t search_size = 25;
	char search[search_size];
	const char *kcmdline_param = "initcall_debug";
	struct tst_kcmdline_var params = TST_KCMDLINE_INIT(kcmdline_param);

	tst_res(TDEBUG, "Check module initialization");
	tst_res(TWARN, "NEED TO IMPORVE FUNCTION tst_kcmdline_parse for single cmdline parameter!!!");
	tst_kcmdline_parse(&params, 1);
	tst_res(TDEBUG, "Check params value %s", params.value);
	tst_res(TDEBUG, "Check params found %b", params.found);
	tst_res(TDEBUG, "Check params key %s", params.key);
	if (params.found)
		tst_res(TINFO, "Cmdline contains '%s'.", kcmdline_param);
	else {
		// ASK: Missing initcall_debug - blocked and finish test or warning?
		tst_res(TWARN, "Improper configuration. Missing cmdline parameter: '%s'.", kcmdline_param);
	};

	snprintf(search, search_size, "[%s] returned 0", module);
	tst_res(TDEBUG, "Search for: %s", search);

	if (IS_FOUND_IN_DMESG(search, marker))
		tst_res(TPASS, "Module '%s' properly initialized.", module);
	else {
		// ASK: Unproperly initialization module - blocked and finish test or warining?
		tst_res(TWARN, "Module '%s' failed during initialization.", module);
	};
};

static void load_module_successfully(const char *const module)
{
	const char *mgs = "Not loading module. Module %s is built in driver.";

	if (tst_check_builtin_driver(module) == 0)
		tst_res(TINFO, mgs, module);
	else {
		char *marker = NULL;

		if (!is_module_enabled(module)) {

			marker = generate_marker();
			tst_res(TDEBUG, "New marker for module initialization: %s", marker);
			write_to_dmesg(marker);
			tst_res(TDEBUG, "Loading %s module", module);
			tst_modprobe(module, NULL);
			if (!is_module_enabled(module))
				tst_brk(TBROK, "CONF ERROR: Cannot load %s module", module);
			MCE_INJECT_MODULE_UNLOAD = true;
		};
		check_module_initialization(module, marker);
		free(marker);
	};
};

static bool is_found(const char *const cmd[], const char *search)
{
	const char *log_path = "is_found.log";
	bool found = false;

	if (tst_cmd(cmd, log_path, NULL, TST_CMD_PASS_RETVAL) == 0)
		found = find_in_file(log_path, search);

	remove(log_path);
	return found;
};

static bool is_mcelog_daemon_run(void)
{
	const char *const cmd_psaux_mcelog[] = {"ps", "aux", NULL};
	bool daemon_run;

	tst_res(TDEBUG, "Check if mce daemon is running");
	daemon_run = is_found(cmd_psaux_mcelog, MCELOG_DAEMON);
	if (daemon_run)
		tst_res(TDEBUG, "mcelog daemon running");
	else
		tst_res(TDEBUG, "mcelog daemon not running");
	return daemon_run;
};

static void kill_mce_daemon(void)
{
	const char *const cmd_pkill[] = {"pkill", "-f", MCELOG_DAEMON, NULL};

	tst_res(TDEBUG, "Kill melog daemon process");
	if (tst_cmd(cmd_pkill, NULL, NULL, TST_CMD_PASS_RETVAL) != 0)
		tst_brk(TBROK, "CONF ERROR: Cannot kill mcelog daemon process.");
};

static void deactive_mce_daemon(void)
{
	tst_res(TDEBUG, "Deactivate mcelog daemon");
	if (is_mcelog_daemon_run()) {
		kill_mce_daemon();
		if (is_mcelog_daemon_run())
			tst_brk(TBROK, "CONF ERROR: Cannot deactivate mcelog with daemon mode.");
		tst_res(TDEBUG, "%s - stopped", MCELOG_DAEMON);
	} else
		tst_res(TINFO, "%s - not running", MCELOG_DAEMON);
};

static void load_mce_daemon(void)
{
	const char *const cmd_mcelog_daemon_run[] = {"mcelog", "--daemon", "--logfile", LOG_MCE_FILE, NULL};

	tst_res(TDEBUG, "Load mce daemon");
	deactive_mce_daemon();

	if (tst_cmd(cmd_mcelog_daemon_run, NULL, NULL, TST_CMD_PASS_RETVAL) != 0)
		tst_brk(TBROK, "CONF ERROR: Cannot run mcelog daemon!");

	if (!is_mcelog_daemon_run())
		tst_brk(TBROK, "CONF ERROR: mcelog daemon not running properly!");

	tst_res(TINFO, "Successfully load melog daemon.");
};

static void setup(void)
{
	char *marker = generate_marker();

	tst_res(TDEBUG, "Setup");

	MCE_INJECT_MODULE_UNLOAD = false;
	TMP_MCE_DIR = tst_tmpdir_path();
	tst_res(TDEBUG, "Temporary dir: %s", TMP_MCE_DIR);
	write_to_dmesg(marker);
	LOG_MCE_FILE = get_logfile(marker);

	load_module_successfully(MCE_INJECT_MODULE);
	load_mce_daemon();

	check_dmesg_errors(marker);
	free(marker);
};

static void check_and_compare_cpu_values(int cpuvalue, int mcevalue, char *logtype)
{
	tst_res(TDEBUG, "Test: Compare values %s", logtype);
	if (mcevalue != -1 && mcevalue == cpuvalue)
		tst_res(TPASS, "CPUID %s show correctly by mcelog message.", logtype);
	else {
		tst_res(TFAIL, "CPUID %s uncorrectly show by mcelog message.", logtype);
		tst_res(TDEBUG, "CPUID value: %d. MCE CPU value %d.", cpuvalue, mcevalue);
	};
}

static void check_cpu_support(int cpu_family, int cpu_model)
{
	const char *const cmd_cpu_support[] = {"mcelog", "--is-cpu-supported", NULL};
	const char *log_path = "cpu_support_log_path.log";
	const char *full_support_msg = "mcelog: Family %d Model %d CPU: only decoding architectural errors";
	const char *support_msg = "only decoding architectural errors";
	int mce_family = -1, mce_model = -1;
	char *marker = generate_marker();

	tst_res(TINFO, "Test: Check CPU support by mcelog.");

	if (tst_cmd(cmd_cpu_support, NULL, log_path, TST_CMD_PASS_RETVAL) != 0)
		tst_brk(TBROK, "Command 'mcelog --is-cpu-supported' not run properly.");

	if (!find_in_file(log_path, support_msg))
		tst_res(TPASS, "CPU supported by mcelog.");
	else {
		if (SAFE_FILE_LINES_SCANF(log_path, full_support_msg, &mce_family, &mce_model) == 0) {
			check_and_compare_cpu_values(cpu_family, mce_family, "Family");
			check_and_compare_cpu_values(cpu_model, mce_model, "Model");
		} else
			tst_res(TFAIL, "Unpropertly show mce error message.");
		remove(log_path);
		tst_brk(TFAIL, "CPU not supported by mcelog - %s", support_msg);
	};
	remove(log_path);
	check_dmesg_errors(marker);
	free(marker);
};

static void mceinject_test(int cpu_family, int cpu_model, int cpu_step)
{
	const char *test_corrected = "testcases/data/mce/mce_test_corrected";
	const size_t test_path_size = 252;
	char test_path[test_path_size];

	snprintf(test_path, test_path_size, "%s/%s", LTPROOT, test_corrected);

	const char *const cmd_test_run[] = {"mce-inject", test_path, NULL};
	const char *search_mce_msg = "CPUID Vendor Intel Family %d Model %d Step %d";
	const char *find_mce_msg = "MCE CPU values: Family %d, Model %d, Stepping %d";
	char *marker = generate_marker();
	int mce_family = -1, mce_model = -1, mce_step = -1;

	tst_res(TINFO, "Test: MCELOG inject test");
	tst_cmd(cmd_test_run, NULL, NULL, TST_CMD_PASS_RETVAL);

	if (SAFE_FILE_LINES_SCANF(LOG_MCE_FILE, search_mce_msg, &mce_family, &mce_model, &mce_step) != 0)
		tst_res(TFAIL, "Something wrong with mcelog CPUID error information.");
	else {
		tst_res(TPASS, "CPU information correctly show in mcelog errors.");
		tst_res(TDEBUG, find_mce_msg, mce_family, mce_model, mce_step);
		check_and_compare_cpu_values(cpu_family, mce_family, "Family");
		check_and_compare_cpu_values(cpu_model, mce_model, "Model");
		check_and_compare_cpu_values(cpu_step, mce_step, "Step");
	};

	check_dmesg_errors(marker);
	free(marker);
};

static void run(void)
{
	const char *tested_cpu_info = "Tested cpu family: %d, model: %d, stepping %d";
	int cpu_family = -1, cpu_model = -1, cpu_step= -1;

	cpu_family = SAFE_READ_CPUINFO("cpu family");
	cpu_model = SAFE_READ_CPUINFO("model");
	cpu_step = SAFE_READ_CPUINFO("stepping");
	tst_res(TDEBUG, tested_cpu_info, cpu_family, cpu_model, cpu_step);

	check_cpu_support(cpu_family, cpu_model);

	mceinject_test(cpu_family, cpu_model, cpu_step);
};

static void cleanup(void)
{
	deactive_mce_daemon();
	if (MCE_INJECT_MODULE_UNLOAD) {
		tst_res(TDEBUG, "Unload module %s", MCE_INJECT_MODULE);
		tst_module_unload_(NULL, MCE_INJECT_MODULE);
		is_module_enabled(MCE_INJECT_MODULE);
	};
};

static struct tst_test test = {
	.needs_root = 1,
	.needs_tmpdir = 1,
	.needs_kconfigs = (const char *[]) {
		"CONFIG_X86_MCE",
		"CONFIG_X86_MCELOG_LEGACY",
		"CONFIG_X86_MCE_INJECT",
	},
	.needs_cmds = (struct tst_cmd[]) {
		{.cmd = "mcelog"},
		{.cmd = "mce-inject"},
		{}
	},
	.supported_archs = (const char *const []) {
		"x86",
		"x86_64",
		NULL
	},
	.setup = setup,
	.test_all = run,
	.cleanup = cleanup,
	.taint_check = TST_TAINT_W | TST_TAINT_D | TST_TAINT_M | TST_TAINT_C |
	TST_TAINT_I | TST_TAINT_O | TST_TAINT_E | TST_TAINT_L,
};
