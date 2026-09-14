// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2026 Piotr Kubaj <piotr.kubaj@intel.com>
 */

/*
 * Test that tst_test.needs_debugfs gets debugfs mounted for the test.
 *
 * Run it both with debugfs mounted and with it unmounted, it has to pass either
 * way. In the latter case the library reports mounting debugfs and has to leave
 * it unmounted once the test is over.
 */

#include <sys/vfs.h>
#include "tst_test.h"

static void run(void)
{
	struct statfs sfs;

	SAFE_STATFS(TST_DEBUGFS_PATH, &sfs);

	if (sfs.f_type == TST_DEBUGFS_MAGIC)
		tst_res(TPASS, "debugfs is mounted at %s", TST_DEBUGFS_PATH);
	else
		tst_res(TFAIL, "debugfs is not mounted at %s", TST_DEBUGFS_PATH);
}

static struct tst_test test = {
	.needs_root = 1,
	.needs_debugfs = 1,
	.needs_kconfigs = (const char *const []) {
		"CONFIG_DEBUG_FS",
		NULL
	},
	.test_all = run,
};
