// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Intel - http://www.intel.com/
 * Copyright (C) 2026 Tomasz Ossowski tomasz.ossowski@intel.com
 */

/*
 * Verify that platform supports GPE/PME acpi events.
 * Platform shall support SCI/PME events from PCH devices using GPIO + _AEI () method
 *
 */

#include "tst_test.h"
#include "tst_safe_stdio.h"

static bool search_pattern(const char *pattern)
{
	struct dirent *dp;
	DIR *dir = SAFE_OPENDIR("./");

	if (!dir)
		return false;

	while ((dp = SAFE_READDIR(dir)) != NULL) {
		if (strstr(dp->d_name, ".dsl"))	{
			char line_data[PATH_MAX];
			FILE *file = SAFE_FOPEN(dp->d_name, "r");

			while (fgets(line_data, sizeof(line_data), file)) {
				if (strstr(line_data, pattern))	{
					tst_res(TINFO, "Found %s on file %s", pattern, dp->d_name);
					SAFE_FCLOSE(file);
					return true;
				}
			}
			SAFE_FCLOSE(file);
		}
	}
	SAFE_CLOSEDIR(dir);
	return false;
}

static void iterate_iasl_command_on_files(char *filter_format)
{
	struct dirent *dp;
	DIR *dir = SAFE_OPENDIR("/sys/firmware/acpi/tables/");

	while ((dp = SAFE_READDIR(dir)) != NULL) {
		if (strstr(dp->d_name, filter_format)) {
			char path_source[PATH_MAX];
			char path_output[PATH_MAX];

			snprintf(path_source, sizeof(path_source), "/sys/firmware/acpi/tables/%s", dp->d_name);
			snprintf(path_output, sizeof(path_output), "./%s", dp->d_name);

			const char *const tmp_cmd_str[] = {"iasl", "-p", path_output, "-d", path_source, NULL};

			tst_cmd(tmp_cmd_str, NULL, NULL, TST_CMD_PASS_RETVAL);
		}
	}
	SAFE_CLOSEDIR(dir);
}

static void run(void)
{
	iterate_iasl_command_on_files("SSDT");

	if (search_pattern("PGP1"))	{
		tst_res(TPASS, "Found PGP1 annotation regarding to GPE/PME device. Test passed");
		return;
	}

	tst_res(TFAIL, "No any GPE/PME device found");
}

static struct tst_test test = {
	.min_runtime = 30,
	.needs_root = 1,
	.supported_archs = (const char *const[]){
		"x86",
		"x86_64",
		NULL},
	.needs_cmds = (struct tst_cmd[]){{.cmd ="acpidump"}, {.cmd ="acpixtract"}, {.cmd ="iasl"}, {}},
	.needs_tmpdir = 1,
	.test_all = run
};
