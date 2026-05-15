// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Helena Anna Dubel <helena.anna.dubel@intel.com>
 */

/*\
 * Checks mcelog support for platform CPU model and family number.
 * Checks properly show CPU model and family number in mcelog messages and
 * errors.
 */

#include <stdio.h>

#include "tst_test.h"
#include "tst_safe_stdio.h"
#include "tst_safe_file_ops.h"
#include "tst_module.h"
#include "tst_kconfig.h"
#include "tst_cpu.h"

#define MCE_INJECT_MODULE "mce_inject"

static char *get_tmpfile(char *name)
{
	char *tmp_path = tst_tmpdir_path();
	const size_t file_size = strlen(tmp_path) + strlen(name) + 32;
	char *file = tst_alloc(file_size);

	snprintf(file, file_size, "%s/mcelog-%s.txt", tmp_path, name);
	return file;
};

static void check_module_initialization(void)
{
	struct tst_kcmdline_var params = TST_KCMDLINE_INIT("initcall_debug");

	tst_res(TDEBUG, "Check module initialization");
	tst_kcmdline_parse(&params, 1);
	if (!params.found)
		tst_res(TWARN, "Improper configuration. Missing cmdline parameter.");
};

static void load_module(const char *const mod)
{
	if (tst_check_builtin_driver(mod) == 0) {
		tst_res(TINFO, "%s built-in, skipping load", mod);
		return;
	}

	if (!tst_is_module_loaded(mod)) {
		tst_res(TDEBUG, "Loading module %s", mod);
		tst_modprobe(mod, NULL);

		if (!tst_is_module_loaded(mod))
			tst_brk(TBROK, "CONF ERROR: Failed to load module %s", mod);

	};
	check_module_initialization();
};

static void stop_mcelog(void)
{
	int pid = -1;

	FILE_SCANF("/var/run/mcelog.pid", "%d\n", &pid);
	if (pid == -1) {
		tst_res(TDEBUG, "mcelog daemon not running.");
		return;
	};

	char pid_str[32];

	snprintf(pid_str, sizeof(pid_str), "%d", pid);

	const char *const cmd[] = {"kill", pid_str, NULL};

	tst_res(TDEBUG, "Killing melog daemon process - pid: %d", pid);
	if (tst_cmd(cmd, NULL, NULL, TST_CMD_PASS_RETVAL) != 0)
		tst_brk(TBROK, "CONF ERROR: Failed to kill mcelog daemon process.");

	tst_res(TINFO, "melog daemon process - killed!");
	
};

static void start_mcelog(const char *logfile)
{
	const char *const cmd[] = {"mcelog", "--daemon", "--logfile", logfile, NULL};

	tst_res(TDEBUG, "Starting mcelog daemon");
	if (tst_cmd(cmd, NULL, NULL, TST_CMD_PASS_RETVAL) != 0)
		tst_brk(TBROK, "CONF ERROR: Failed to start mcelog! Check if mcelog is already running.");
	
	tst_res(TINFO, "Successfully load melog daemon.");
};

static void check_and_compare_cpu_values(int cpuvalue, int mcevalue, char *logtype)
{
	tst_res(TDEBUG, "Test: Compare values %s", logtype);
	if (mcevalue != -1 && mcevalue == cpuvalue)
		tst_res(TPASS, "CPUID %s printed correctly by mcelog message.", logtype);
	else {
		tst_res(TFAIL, "CPUID %s uncorrectly printed by mcelog message.", logtype);
		tst_res(TDEBUG, "CPUID value: %d. MCE CPU value %d.", cpuvalue, mcevalue);
	};
}

static void check_cpu_support(int cpu_family, int cpu_model)
{
	tst_res(TINFO, "Test 1: Check CPU support by mcelog.");

	const char *const cmd[] = {"mcelog", "--is-cpu-supported", NULL};
	const char *log_path = get_tmpfile("cpu_support");

	if (tst_cmd(cmd, NULL, log_path, TST_CMD_PASS_RETVAL) != 0)
		tst_brk(TBROK, "Command 'mcelog --is-cpu-supported' not run properly.");
	
	int c = fgetc(SAFE_FOPEN(log_path, "r"));
	
	if (c == "\n" || c == EOF) {
		tst_res(TPASS, "CPU supported by mcelog.");
		return;
	};

	int mce_family = -1, mce_model = -1;
	const char *msg = "mcelog: Family %d Model %d CPU: only decoding architectural errors";

	if (SAFE_FILE_LINES_SCANF(log_path, msg, &mce_family, &mce_model) == 0) {
		check_and_compare_cpu_values(cpu_family, mce_family, "Family");
		check_and_compare_cpu_values(cpu_model, mce_model, "Model");
	} else
		tst_res(TFAIL, "Improperly displayed MCE error message");

	tst_brk(TFAIL, "CPU not supported by mcelog - only decoding architectural errors");
};

static void mceinject_test(const char *log_mce_file, int cpu_family, int cpu_model, int cpu_step)
{
	const char *test_corrected = "testcases/data/mce/mce_test_corrected";
	const size_t test_path_size = 252;
	char test_path[test_path_size];

	snprintf(test_path, test_path_size, "%s/%s", "/opt/ltp", test_corrected);

	const char *const cmd[] = {"mce-inject", test_path, NULL};
	const char *search_mce_msg = "CPUID Vendor Intel Family %d Model %d Step %d";
	const char *find_mce_msg = "MCE CPU values: Family %d, Model %d, Stepping %d";
	int mce_family = -1, mce_model = -1, mce_step = -1;

	tst_res(TINFO, "Test 2: MCELOG inject test");
	if (tst_cmd(cmd, NULL, NULL, TST_CMD_PASS_RETVAL) != 0)
		tst_brk(TFAIL, "Failed to inject an error.");

	if (SAFE_FILE_LINES_SCANF(log_mce_file, search_mce_msg, &mce_family, &mce_model, &mce_step) != 0)
		tst_res(TFAIL, "Something wrong with mcelog CPUID error information.");
	else {
		tst_res(TPASS, "CPU information correctly show in mcelog errors.");
		tst_res(TDEBUG, find_mce_msg, mce_family, mce_model, mce_step);
		check_and_compare_cpu_values(cpu_family, mce_family, "Family");
		check_and_compare_cpu_values(cpu_model, mce_model, "Model");
		check_and_compare_cpu_values(cpu_step, mce_step, "Step");
	};
};

static void cleanup(bool unload_module)
{
	stop_mcelog();

	if (unload_module) {
		tst_res(TDEBUG, "Unload module %s", MCE_INJECT_MODULE);
		tst_module_unload(MCE_INJECT_MODULE);
		if (tst_is_module_loaded(MCE_INJECT_MODULE))
			tst_brk(TFAIL, "Failed to unload module %s." MCE_INJECT_MODULE);
	};
};


static void run(void)
{
	int cpu_family = -1, cpu_model = -1, cpu_step = -1;
	const char *log_mce_file = get_tmpfile("logfile");
	bool needs_to_unload_module = tst_is_module_loaded(MCE_INJECT_MODULE);

	tst_parse_int(tst_get_cpuinfo(0, "cpu family"), &cpu_family, 0, INT_MAX);
	tst_parse_int(tst_get_cpuinfo(0, "model"), &cpu_model, 0, INT_MAX);
	tst_parse_int(tst_get_cpuinfo(0, "stepping"), &cpu_step, 0, INT_MAX);

	cleanup(false);

	load_module(MCE_INJECT_MODULE);
	start_mcelog(log_mce_file);
	check_cpu_support(cpu_family, cpu_model);
	mceinject_test(log_mce_file, cpu_family, cpu_model, cpu_step);

	cleanup(needs_to_unload_module);
};

static struct tst_test test = {
	.test_all = run,

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
		"x86_64",
		NULL
	},
};
