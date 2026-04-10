// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Helena Anna Dubel <helena.anna.dubel@intel.com>
 */

/*\
 * Checks mcelog support for platform CPU model and family number.
 * Checks properly show CPU model and family number in mcelog messages and
 * errors.
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
#include "tst_cpu.h"
#include "tst_safe_stdio.h"
#include "tst_safe_file_ops.h"

#define TNAME "mcelog-cpu-model-number-support"

#define MCE_INJECT_MODULE "mce_inject"
#define MCELOG_DAEMON "mcelog --daemon"
#define LTPROOT "/opt/ltp"

static char *TMP_MCE_DIR;
static char *LOG_MCE_FILE;
static bool NEEDS_TO_UNLOAD_MODULE;

static char *get_tmpfile(char *name, char *marker)
{
	const size_t file_size = strlen(TMP_MCE_DIR) + strlen(marker) + strlen(name) + 25;
	char *file = tst_alloc(file_size);

	snprintf(file, file_size, "%s/mcelog-%s-%s.txt", TMP_MCE_DIR, name, marker);
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

static void load_module_successfully(const char *const module)
{
	if (tst_check_builtin_driver(module) == 0)
		tst_res(TINFO, "Nothing to do. Module %s is built in driver.", module);
	else {
		if (!tst_is_module_loaded(module)) {
			tst_res(TDEBUG, "Load %s module", module);
			tst_modprobe(module, NULL);
			if (!tst_is_module_loaded(module))
				tst_brk(TBROK, "CONF ERROR: Cannot load %s module", module);
			NEEDS_TO_UNLOAD_MODULE = true;
		};
		check_module_initialization();
	};
};

static bool is_mcelog_daemon_run(void)
{
	const char *const cmd_psaux_mcelog[] = {"ps", "aux", NULL};
	bool daemon_run = FIND_IN_CMD_EXEC(cmd_psaux_mcelog, MCELOG_DAEMON);

	tst_res(TDEBUG, "Check if mce daemon is running");
	if (daemon_run)
		tst_res(TINFO, "%s - is running", MCELOG_DAEMON);
	else
		tst_res(TINFO, "%s - is not running", MCELOG_DAEMON);
	return daemon_run;
};

static void kill_mce_daemon(void)
{
	const char *const cmd_pkill[] = {"pkill", "-f", MCELOG_DAEMON, NULL};

	tst_res(TDEBUG, "Kill melog daemon process");
	if (tst_cmd(cmd_pkill, NULL, NULL, TST_CMD_PASS_RETVAL) != 0)
		tst_brk(TBROK, "CONF ERROR: Cannot kill mcelog daemon process.");
	tst_res(TINFO, "melog daemon process - killed!");
};

static void deactive_mce_daemon(void)
{
	if (is_mcelog_daemon_run()) {
		kill_mce_daemon();
		if (is_mcelog_daemon_run())
			tst_brk(TBROK, "CONF ERROR: Cannot deactivate mcelog with daemon mode.");
		tst_res(TDEBUG, "%s - deactivated successfully", MCELOG_DAEMON);
	}
};

static void load_mce_daemon(void)
{
	const char *const cmd_mcelog_daemon_run[] = {"mcelog", "--daemon", "--logfile", LOG_MCE_FILE, NULL};

	deactive_mce_daemon();

	tst_res(TDEBUG, "Load mce daemon");
	if (tst_cmd(cmd_mcelog_daemon_run, NULL, NULL, TST_CMD_PASS_RETVAL) != 0)
		tst_brk(TBROK, "CONF ERROR: Cannot run mcelog daemon!");

	if (!is_mcelog_daemon_run())
		tst_brk(TBROK, "CONF ERROR: mcelog daemon not running properly!");

	tst_res(TINFO, "Successfully load melog daemon.");
};

static void setup(void)
{
	NEEDS_TO_UNLOAD_MODULE = false;
	TMP_MCE_DIR = tst_tmpdir_path();
	LOG_MCE_FILE = get_tmpfile("logfile", "logmarker");

	load_module_successfully(MCE_INJECT_MODULE);
	load_mce_daemon();
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
	const char *log_path = get_tmpfile("cpu_support", "marker");
	const char *full_support_msg = "mcelog: Family %d Model %d CPU: only decoding architectural errors";
	const char *support_msg = "only decoding architectural errors";
	int mce_family = -1, mce_model = -1;

	tst_res(TINFO, "Test 1: Check CPU support by mcelog.");
	if (tst_cmd(cmd_cpu_support, NULL, log_path, TST_CMD_PASS_RETVAL) != 0)
		tst_brk(TBROK, "Command 'mcelog --is-cpu-supported' not run properly.");

	if (!FIND_IN_FILE(log_path, support_msg))
		tst_res(TPASS, "CPU supported by mcelog.");
	else {
		if (SAFE_FILE_LINES_SCANF(log_path, full_support_msg, &mce_family, &mce_model) == 0) {
			check_and_compare_cpu_values(cpu_family, mce_family, "Family");
			check_and_compare_cpu_values(cpu_model, mce_model, "Model");
		} else
			tst_res(TFAIL, "Unpropertly show mce error message.");
		tst_brk(TFAIL, "CPU not supported by mcelog - %s", support_msg);
	};
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
	int mce_family = -1, mce_model = -1, mce_step = -1;

	tst_res(TINFO, "Test 2: MCELOG inject test");
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
};

static void run(void)
{
	const char *tested_cpu_info = "Tested cpu family: %d, model: %d, stepping %d";
	int cpu_family = -1, cpu_model = -1, cpu_step = -1;

	tst_parse_int(tst_get_cpuinfo(0, "cpu family"), &cpu_family, 0, INT_MAX);
	tst_parse_int(tst_get_cpuinfo(0, "model"), &cpu_model, 0, INT_MAX);
	tst_parse_int(tst_get_cpuinfo(0, "stepping"), &cpu_step, 0, INT_MAX);
	tst_res(TDEBUG, tested_cpu_info, cpu_family, cpu_model, cpu_step);

	check_cpu_support(cpu_family, cpu_model);

	mceinject_test(cpu_family, cpu_model, cpu_step);
};

static void cleanup(void)
{
	deactive_mce_daemon();
	if (NEEDS_TO_UNLOAD_MODULE) {
		tst_res(TDEBUG, "Unload module %s", MCE_INJECT_MODULE);
		tst_module_unload_(NULL, MCE_INJECT_MODULE);
		tst_is_module_loaded(MCE_INJECT_MODULE);
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
};
