#!/usr/bin/env python3

# Helena Anna Dubel <helena.anna.dubel@intel.com>

import sys
import subprocess
import re
import time
from pathlib import Path

CMDLINE_FILE = "/proc/cmdline"
CPUINFO_FILE = "/proc/cpuinfo"
DMESG_FILE = "/dev/kmsg"
TEST_START = None
t = time.strftime("%Y%m%d%H%M%S")
DMESG_TEST_MSG = f"TEST MCELOG START - {t}"
LOG_PATH = Path("/tmp/log/")
LOG_FILE = LOG_PATH / f"mcelog-{t}"
MCELOG_DAEMON_CMD = f"mcelog --daemon --logfile={LOG_FILE}"

class TestError(Exception):
    pass

def run_cmd(cmd, sudo=True, check=True):
    if sudo:
        cmd = f"sudo {cmd}"
    p = subprocess.run(cmd, shell=True, text=True, check=check,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and p.returncode != 0:
        print(p.stdout)
        raise TestError(f"Cannot run command: {cmd}")
    return p

def dmesg_inject(msg):
    run_cmd(f"echo {msg} > {DMESG_FILE}")
    result = run_cmd(f"dmesg --time-format=iso | grep '{msg}'")
    return result.stdout.splitlines()[0].split(" ")[0].split(",")[0]

def module_is_loaded(module):
    if run_cmd(f"lsmod | grep '{module}'", check=False).returncode == 0:
        print(f"INFO: Module {module} is loaded.")
        return True

    print(f"INFO: Module {module} is not loaded.")
    return False

def cleanup(leave_loaded=False):
    if not leave_loaded:
        run_cmd("modprobe -r mce_inject")
    run_cmd(f"pkill -f '{MCELOG_DAEMON_CMD}'", check=False)
    file = Path(LOG_FILE)
    if file.exists():
        file.unlink()

def check_tools(tools):
    for i in tools:
        if run_cmd(f"which {i}", check=False).returncode != 0:
            raise TestError(f"Not found '{i}'")

def get_kconfig_value(option):
    kernel = run_cmd("uname -r").stdout.strip("\n")
    pattern = f"{option}="
    value = ""

    try:
        p = run_cmd(f"grep '^{pattern}' /boot/config-{kernel}")
        if not p.stdout:
            raise TestError("Option not found in kconfig.")
        value = p.stdout.split(pattern)[1].strip("\n")
        if value == "y":
            print(f"INFO: {option} built-in kernel.")
        elif value == "m":
            print(f"INFO: {option} build as module.")
        else:
            raise TestError(f"Missing kconfig or unknown value: {option}={value}")
    except IndexError as exc:
        raise TestError("Option not found in kconfig.") from exc

    return value

def check_module_initialization(module):
    init_param = "initcall_debug"
    if run_cmd(f"grep '{init_param}' {CMDLINE_FILE}", check=False).returncode != 0:
        print(f"WARN: Cannot check initialization, {init_param} cannot found in {CMDLINE_FILE}.")
        return

    result = run_cmd("dmesg | grep -i '\\[{module}\\] returned 0' | tail -1", check=False)
    if result.returncode == 0:
        print(f"INFO: {module} successfully initialized.")
    else:
        print(f"INFO: {module} initialization fails.")

def check_dmesg_errors(pattern):
    errors = ("error", "fail", "warn")
    p = run_cmd(f"dmesg --since '{TEST_START}' | grep '{pattern}'", check=False)
    for line in p.stdout.splitlines():
        if any(e in line for e in errors):
            print("WARN: Found unexpected logs in dmesg.")
            print(p.stdout)

def load_module(module):
    if not module_is_loaded(module):
        print("INFO: Load module")
        run_cmd(f"modprobe {module}")
        if not module_is_loaded(module):
            raise TestError(f"Cannot load module {module}")

    check_module_initialization(module)
    check_dmesg_errors(module)

def prepare():
    check_tools(["mcelog", "mce-inject"])
    get_kconfig_value("CONFIG_X86_MCELOG_LEGACY")
    if get_kconfig_value("CONFIG_X86_MCE_INJECT") == "m":
        load_module("mce_inject")
    if not LOG_PATH.exists():
        LOG_PATH.mkdir(parents=True, exist_ok=True)
    if not LOG_FILE.exists():
        LOG_FILE.touch()

def get_cpuinfo(item):
    try:
        result = run_cmd(f"grep '{item}' {CPUINFO_FILE}")
        return result.stdout.splitlines()[0].split(":")[1].strip()
    except IndexError as exc:
        raise TestError(f"Cannot get {item} for {CPUINFO_FILE}.") from exc

def get_mceinfo(line, item):
    try:
        return re.search(item + r" \d+", line).group().split(" ")[1]
    except IndexError as exc:
        raise TestError(f"Cannot get {item} for {line}.") from exc

def check_mcelog_output():
    p = run_cmd(f"grep 'CPUID Vendor' {LOG_FILE}")
    if not p.stdout:
        raise TestError("Cannot find required information in mcelog.")

    mce_line = p.stdout.splitlines()[0]
    if any(k in mce_line for k in ("unknow", "%u")):
        raise TestError("Found unexpected patterns.")

    cpu_model, mce_model = get_cpuinfo("^model"), get_mceinfo(mce_line, "Model")
    if cpu_model != mce_model:
        raise TestError(f"Compare Model number fails: {cpu_model} != {mce_model}.")

    cpu_family, mce_family = get_cpuinfo("^cpu family"), get_mceinfo(mce_line, "Family")
    if cpu_family != mce_family:
        raise TestError(f"Compare Family number fails: {cpu_family} != {mce_family}.")

    cpu_step, mce_step = get_cpuinfo("stepping"), get_mceinfo(mce_line, "Step")
    if cpu_step != mce_step:
        raise TestError(f"Compare Stepping fails: {cpu_step} != {mce_step}.")

    print("INFO: CPU model, family number and stepping printed as expected.")

def mcelog_test():
    mce_msg = run_cmd("mcelog --is-cpu-supported").stdout
    if "only decoding architectural errors" in mce_msg:
        raise TestError(f"CPU is not fully supported - {mce_msg}")
    print("INFO: CPU is supported by mcelog")

    run_cmd(MCELOG_DAEMON_CMD)
    run_cmd("mce-inject corrected")
    check_mcelog_output()
    check_dmesg_errors("mce")

    print("TEST PASS")
    
def run_test():
    TEST_START = dmesg_inject(DMESG_TEST_MSG)
    prepare()
    mcelog_test()

if __name__ == "__main__":
    try:
        leave_loaded = module_is_loaded("mce_inject")
        cleanup()
        run_test()
        cleanup(leave_loaded)
    except (subprocess.CalledProcessError, TestError) as e:
        print(f"FAIL: {e}")
        cleanup(leave_loaded)
        sys.exit(1)
