#!/bin/bash
# SPDX-License-Identifier: GPL-2.0
# Author: Tomasz, Ossowski <tomasz.ossowski@intel.com>
#  Functions for handling Cswitch-control utility
#

readonly LTP_TC_BIN_PATH="$LTPROOT/testcases/bin/"
readonly CSWITCH_PATH="ccd-linux-tools/cswitch-control/pyftdi"
readonly PYTHON3_FTDI_PATH="/usr/lib/python3/dist-packages/pyftdi"
readonly LIBUSB_PATH="/usr/include/libusb-1.0/libusb.h"

readonly PYTHON3_FTDI_TOOL_NAME="python3-ftdi"
readonly LIBUSB_TOOL_NAME="libusb-1.0"

readonly APT_GET_INSTALL="apt-get install"
readonly PYTHON="python3"

readonly CSWITCH_CONTROL="cswitch_control.py"
readonly CCD_CSWITCH_CONTROL="ccd_cswitch_control.py"

readonly PRODUCT_DESCRIPTOR="Product descriptor: "
readonly LSUSB_PRODUCT_NAME="Future Technology Devices International, Ltd FT2232C"
readonly CSWITCH_CONTROL_DEVICE="Cswitch-Control device"

check_exist_cswitch_tool() {
  if [ -n "$CSWITCH_PATH" ] && [ $(ls $CSWITCH_PATH | wc -l) > 1 ]; then
    tst_res TPASS "Path with cswith-control exist."
    if [ -n "$LIBUSB_PATH" ]; then
      tst_res TPASS "Package $LIBUSB_TOOL_NAME exist."
    else
      tst_res TINFO "Package $LIBUSB_TOOL_NAME is not installed. Trying to install"
      $($APT_GET_INSTALL $LIBUSB_TOOL_NAME)
    fi

    if [ -n "$PYTHON3_FTDI_PATH" ] && [ $(ls $PYTHON3_FTDI_PATH | wc -l) > 1 ]; then
      tst_res TPASS "Package $PYTHON3_FTDI_TOOL_NAME exist."
    else
      tst_res TINFO "Package $PYTHON3_FTDI_TOOL_NAME is not installed. Trying to install"
      $($APT_GET_INSTALL $PYTHON3_FTDI_TOOL_NAME)
    fi
  else
    tst_res TCONF "Cannot find Cswitch-control tool."
    return 1
  fi

  return 0
}

cswitchControl_change_port() {
  port=$1

  check_exist_cswitch_tool
  listProductdescriptor=$($PYTHON $LTP_TC_BIN_PATH/$CSWITCH_PATH/$CSWITCH_CONTROL -l \
                        | grep "$PRODUCT_DESCRIPTOR" \
                        | cut -b $(echo "$PRODUCT_DESCRIPTOR" | wc -c )-)

  if [[ -z "$listProductdescriptor" ]]; then
    tst_res TCONF "Cannot find product description"
    return 1
  fi

  $($PYTHON $LTP_TC_BIN_PATH/$CSWITCH_PATH/$CCD_CSWITCH_CONTROL -p $port -d "$listProductdescriptor")
  tst_res TPASS "Executed switching port id to $port on $CSWITCH_CONTROL_DEVICE"
}

is_exist_any_cswitch_device() {
  anyCswtch=$(lsusb | grep "$LSUSB_PRODUCT_NAME")
  if [ -z "$anyCswtch" ]; then
    return 1
  fi
    
  tst_res TPASS "Found $CSWITCH_CONTROL_DEVICE device from lsusb"
  return 0
}

. tst_test.sh