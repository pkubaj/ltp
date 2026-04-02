// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2026 Piotr Kubaj <piotr.kubaj@intel.com>
 */

/*\
 * Validate presence and permissions of RFIM attributes.
 * The test checks first validity of general RFIM attributes,
 * and then checks either DLVR or FIVR, depending on hardware.
 */

#include "tst_test.h"

#define RFIM_ROOT "/sys/bus/pci/devices/0000:00:04.0"

static bool dlvr;

static void setup(void)
{
	struct stat stats;

	if (!stat(RFIM_ROOT"/dlvr", &stats)) {
		if (S_ISDIR(stats.st_mode))
			dlvr = true;
		else
			tst_brk(TBROK, "%s exists but is not a directory", RFIM_ROOT"/dlvr");
	} else if (!stat(RFIM_ROOT"/fivr", &stats)) {
		if (S_ISDIR(stats.st_mode))
			dlvr = false;
		else
			tst_brk(TBROK, "%s exists but is not a directory", RFIM_ROOT"/fivr");
	} else
		tst_brk(TBROK, "Neither %s nor %s exists", RFIM_ROOT"/dlvr", RFIM_ROOT"/fivr");
}

static void check_read_only(const char *path)
{
	tst_res(TDEBUG, "Checking whether %s is read-only", path);

	int fd = open(path, O_RDONLY);

	if (fd != -1) {
		close(fd);
		tst_res(TPASS, "%s is readable", path);
	} else
		tst_res(TFAIL | TERRNO, "%s can't be read", path);

	fd = open(path, O_WRONLY);
	if (fd != -1) {
		close(fd);
		tst_res(TFAIL, "%s is writable", path);
	} else
		tst_res(TPASS, "%s is read-only", path);

}

static void check_read_write(const char *path)
{
	tst_res(TDEBUG, "Checking whether %s is read-write", path);

	int fd = open(path, O_RDWR);

	if (fd != -1) {
		close(fd);
		tst_res(TPASS, "%s is read-write", path);
	} else
		tst_res(TFAIL | TERRNO, "%s is not read-write", path);
}

static void run(void)
{
	const char * const fivr_nodes[] = {
		RFIM_ROOT"/fivr/vco_ref_code_lo",
		RFIM_ROOT"/fivr/vco_ref_code_hi",
		RFIM_ROOT"/fivr/spread_spectrum_pct",
		RFIM_ROOT"/fivr/spread_spectrum_clk_enable",
		RFIM_ROOT"/fivr/rfi_vco_ref_code",
		RFIM_ROOT"/fivr/fivr_fffc_rev",
		NULL
	};

	const char * const ro_general_nodes[] = {
		RFIM_ROOT"/dvfs/ddr_data_rate_point_0",
		RFIM_ROOT"/dvfs/ddr_data_rate_point_1",
		RFIM_ROOT"/dvfs/ddr_data_rate_point_2",
		RFIM_ROOT"/dvfs/ddr_data_rate_point_3",
		NULL
	};

	const char * const ro_dlvr_nodes[] = {
		RFIM_ROOT"/dlvr/dlvr_hardware_rev",
		RFIM_ROOT"/dlvr/dlvr_freq_mhz",
		RFIM_ROOT"/dlvr/dlvr_pll_busy",
		NULL
	};

	const char * const rw_dlvr_nodes[] = {
		RFIM_ROOT"/dlvr/dlvr_freq_select",
		RFIM_ROOT"/dlvr/dlvr_rfim_enable",
		RFIM_ROOT"/dlvr/dlvr_spread_spectrum_pct",
		RFIM_ROOT"/dlvr/dlvr_control_mode",
		RFIM_ROOT"/dlvr/dlvr_control_lock",
		NULL
	};

	const char * const rw_general_nodes[] = {
		RFIM_ROOT"/dvfs/rfi_restriction_run_busy",
		RFIM_ROOT"/dvfs/rfi_restriction_err_code",
		RFIM_ROOT"/dvfs/rfi_restriction_data_rate_base",
		RFIM_ROOT"/dvfs/rfi_restriction_data_rate",
		NULL
	};

	for (int i = 0; ro_general_nodes[i]; i++)
		check_read_only(ro_general_nodes[i]);

	for (int i = 0; rw_general_nodes[i]; i++)
		check_read_write(rw_general_nodes[i]);

	if (dlvr) {
		tst_res(TINFO, "Checking DLVR");
		for (int i = 0; ro_dlvr_nodes[i]; i++)
			check_read_only(ro_dlvr_nodes[i]);

		for (int i = 0; rw_dlvr_nodes[i]; i++)
			check_read_write(rw_dlvr_nodes[i]);
	} else {
		tst_res(TINFO, "Checking FIVR");
		for (int i = 0; fivr_nodes[i]; i++)
			check_read_write(fivr_nodes[i]);
	}
}

static struct tst_test test = {
	.min_kver = "6.4",
	.needs_kconfigs = (const char *const []) {
		"CONFIG_INT340X_THERMAL",
		NULL
	},
	.needs_root = 1,
	.supported_archs = (const char *const []) {
		"x86",
		"x86_64",
		NULL
	},
	.setup = setup,
	.test_all = run
};
