// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Intel HID platform driver test
 *
 * This test verifies:
 * 1. Presence of Intel ACPI HID device
 * 2. Availability of intel-hid platform driver
 * 3. Device binding to intel-hid driver
 *
 * Input device presence is checked only for informational purposes,
 * as it depends on platform features and userspace configuration.
 *
 * Copyright (C) 2025-2026 Intel
 * Author: Daniel Niestepski
 */

#include <dirent.h>
#include <string.h>
#include <stdio.h>

#include "tst_test.h"
#include "tst_safe_file_ops.h"

#define ACPI_PATH "/sys/bus/acpi/devices"
#define INTEL_HID_DRIVER "/sys/bus/platform/drivers/intel-hid"
#define INPUT_PATH "/sys/class/input"

static int acpi_intel_found;
static int bound_device_found;

static void check_acpi_intel_hid(void)
{
	DIR *dir;
	struct dirent *entry;
	char path[PATH_MAX];
	char hid[32];

	dir = opendir(ACPI_PATH);
	if (!dir)
		tst_brk(TBROK, "Cannot open %s", ACPI_PATH);

	while ((entry = readdir(dir))) {

		if (!strcmp(entry->d_name, ".") ||
		    !strcmp(entry->d_name, ".."))
			continue;

		snprintf(path, sizeof(path), "%s/%s/hid",
			 ACPI_PATH, entry->d_name);

		if (access(path, R_OK))
			continue;

		SAFE_FILE_SCANF(path, "%31s", hid);

		if (!strncmp(hid, "INTC", 4) ||
		    !strncmp(hid, "INT33", 5)) {

			tst_res(TINFO, "Found Intel ACPI HID: %s", hid);
			acpi_intel_found = 1;
			break;
		}
	}

	closedir(dir);

	if (!acpi_intel_found)
		tst_brk(TCONF, "No Intel ACPI HID device detected");
}

static void check_driver_binding(void)
{
	DIR *dir;
	struct dirent *entry;

	dir = opendir(INTEL_HID_DRIVER);
	if (!dir)
		tst_brk(TCONF, "intel-hid driver not present");

	while ((entry = readdir(dir))) {

		if (!strcmp(entry->d_name, ".") ||
		    !strcmp(entry->d_name, "..") ||
		    !strcmp(entry->d_name, "bind") ||
		    !strcmp(entry->d_name, "unbind") ||
		    !strcmp(entry->d_name, "uevent") ||
		    !strcmp(entry->d_name, "module"))
			continue;

		tst_res(TINFO, "Found bound intel-hid device: %s",
			entry->d_name);

		bound_device_found = 1;
		break;
	}

	closedir(dir);

	if (!bound_device_found)
		tst_brk(TFAIL,
			"intel-hid driver present but no bound device");
}

static void check_input_devices(void)
{
	DIR *dir;
	struct dirent *entry;
	int found = 0;

	dir = opendir(INPUT_PATH);
	if (!dir) {
		tst_res(TINFO, "Cannot open %s", INPUT_PATH);
		return;
	}

	while ((entry = readdir(dir))) {

		if (!strncmp(entry->d_name, "event", 5)) {
			found = 1;
			break;
		}
	}

	closedir(dir);

	if (found)
		tst_res(TINFO, "Input event device(s) present");
	else
		tst_res(TINFO,
			"No input event devices found (optional)");
}

static void run(void)
{
	check_acpi_intel_hid();
	check_driver_binding();
	check_input_devices();

	tst_res(TPASS, "Intel HID driver is present and functional");
}

static struct tst_test test = {
	.test_all = run,
	.needs_root = 1,
	.needs_drivers = (const char *const[]) {
		"intel_hid",
		NULL
	},
	.supported_archs = (const char *const[]) {
		"x86",
		"x86_64",
		NULL
	},
};