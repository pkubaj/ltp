#!/usr/bin/env bash
###############################################################################
# Copyright (C) 2025 Intel - http://www.intel.com/
#
# GNU General Public License for more details.
###############################################################################
# Contributors:
#   Piotr Kubaj <piotr.kubaj@intel.com> (Intel)
#     -Initial draft.
###############################################################################

${TESTDIR:-/usr/src/linux/tools/testing/selftests/turbostat}/smi_aperf_mperf.py
ret=$?

if [ $ret -gt 0 ]; then
	ret=1
fi

exit $ret
