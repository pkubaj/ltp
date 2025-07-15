TST_NEEDS_ROOT=1

readonly GENERATION_FILE="/sys/bus/thunderbolt/devices/0-0/generation"
readonly USB4_GEN="4"
readonly USB4_PATH="/sys/bus/thunderbolt/devices"
readonly POWER_CONTROL="power/control"
readonly PCI_DEVICES="/sys/bus/pci/devices"
readonly POWER_STATUS="firmware_node/real_power_state"
readonly MOD_NAME="thunderbolt"
readonly VERIFY_SUCCESS="0x0"
readonly NVM_VERIFY="nvm_authenticate"
readonly TBT_PATH="/sys/bus/thunderbolt/devices"
readonly REGEX_ITEM="-"
readonly MOUNT_FOLDER="/folder_for_test"
readonly RW_FILE="${MOUNT_FOLDER}/test_target_file"
readonly POWER_DISK_NODE="/sys/power/disk"
readonly POWER_STATE_NODE="/sys/power/state"
readonly POWER_MEM_SLEEP="/sys/power/mem_sleep"
readonly POWER_PM_TEST_NODE="/sys/power/pm_test"
readonly USB4V2_DEV_ID="5781"
readonly CONTAIN="contain"
readonly NULL="null"
readonly ACM0="/dev/ttyACM0"
readonly CLX_OUTPUT="/tmp/clx_output.txt"
readonly DEFAULT_SLEEP_TIME=30

AUTHORIZE_FILE="authorized"
# CLE equal to 0 means Cleware could work, otherwise no Cleware or not work
CLE=0
export USB4_VER=""
export TBT_DEV_ID=""
export TBT_DEV_IDS=""
HID="hiddev"
HID_CONTENT=$(ls /dev/usb/*$HID*)
FTDI_LIB_PATH="ftdi/lib"
# For FTDI hot plug thunderbolt device
export LD_LIBRARY_PATH=$FTDI_LIB_PATH
DOMAINX=$(ls $TBT_PATH \
          | grep "-" \
          | grep -v "\-0" \
          | head -n1 \
          | cut -c1)
[[ -n "$DOMAINX" ]] || DOMAINX="0"
TBT_HOST_PATH="/sys/bus/thunderbolt/devices/${DOMAINX}-0"

HOST_EXCLUDE="\-0"
DEVICE_LIST=""
HOST_LIST=""
DOMAIN_LIST=""
DEVICE="device"
HOST="host"
DOMAIN="domain"
AUTO="auto"
USED_DOMAIN=""
EXPECT_D3="D3cold"
TBT_PCIS=""
USB4_DEVICE=""
SNVM="nvm_active0/nvmem"
FNVM="nvm_non_active0/nvmem"
TEMP_DIR=""
SECURITY=""
HID="hiddev"
HID_CONTENT=$(ls /dev/usb/*$HID*)
FREE_SPACE=$(df -Ph /tmp | tail -1 | awk '{print $4}')
FREE_SPACE_ROOT=$(df -Ph /root | tail -1 | awk '{print $4}')
SPACE_RESULT=""
AUTHOR_PATH="/sys/bus/thunderbolt/devices/*/authorized"
DEVICE_NODE=""
NODE=""
SYS_PATH=""
TBT_ROOT_PCI=""
TBT_PCIS=""
DEVICES=""
DEV_FILE="/tmp/tbt_dev"
FAIL="fail"
PASS="pass"
FAIL_ORIGIN=$(dmesg | grep -i $MOD_NAME | grep -i $FAIL)
FAIL_FINAL=""

teardown_handler="usb4_teardown"
usb4_teardown() {
  [[ -d "$TEMP_DIR" ]] && rm -rf "$TEMP_DIR" && tst_res TINFO "rm -rf $TEMP_DIR"
  [[ -z "$TEMP_DIR" ]] || {
    TEMP_DIR=""
    tst_res TINFO "set null for TEMP_DIR"
  }
}

# This function caculate file size in bytes based on input parameters
# Input:
#       $1: block size of the file, with unit
#       $2: block count of the file(optional, default value is 1)
# Output:
#       file size in byte on success
#       empty string on failure
# Usage:
#       caculate_size_in_bytes 18M
#       caculate_size_in_bytes 18MB
#       caculate_size_in_bytes 18M 2
#       caculate_size_in_bytes 18MB 2
caculate_size_in_bytes() {
  local block_size=$1
  local block_count=${2:-1}
  local block_size_unit=""
  local block_size_num=""
  local file_size=""

  block_size_num=${block_size//[a-zA-Z]/}
  block_size_unit=${block_size//$block_size_num/}

  # Convert unit to bytes
  block_size=$(echo "$block_size_num * ${block_size_unit}" | bc)
  file_size=$(echo "($block_size * $block_count)/1" | bc)

  echo "$file_size"
}

# Check sysfs controller is usb4
# Input: none
# Output: return 0 for true, otherwise for false
is_usb4_sysfs() {
  local genera=""

  [[ -e "$GENERATION_FILE" ]] || {
    tst_res TWARN "No sysfs file $GENERATION_FILE"
    return 2
  }
  genera=$(cat $GENERATION_FILE)
  [[ "$genera" == "4" ]] || {
    tst_res TWARN "$GENERATION_FILE is not 4:$genera"
    return 1
  }
  return 0
}

# Find used domain
# Input: none
# Output: return 0 for true, otherwise for false
find_used_domain() {
  USED_DOMAIN=$(ls $USB4_PATH \
              | grep "-" \
              | grep -v "\-0" \
              | head -n1 \
              | cut -c1)
}

# Will list all usb4 sys folders with different type
# Input:
#   $1: device type like "device | host | domain"
# Output: return 0 for true, otherwise for false
list_usb4_sysfolders() {
  local device_type=$1

  case $device_type in
    $DEVICE)
      DEVICE_LIST=$(ls -1 "$USB4_PATH" \
                  | grep "-" \
                  | grep -v "\-0$" \
                  | grep -v "\." \
                  | awk '{ print length(), $0 | "sort -n" }' \
                  | cut -d ' ' -f 2)
      [[ -n "$DEVICE_LIST" ]] || tst_brk TCONF "No device under $USB4_PATH"
      tst_res TINFO "DEVICE_LIST:$DEVICE_LIST"
      ;;
    $HOST)
      HOST_LIST=$(ls -1 "$USB4_PATH" | grep "\-0$")
      [[ -n "$HOST_LIST" ]] || tst_brk TCONF "No host under $USB4_PATH"
      tst_res TINFO "HOST_LIST:$HOST_LIST"
      ;;
    $DOMAIN)
      DOMAIN_LIST=$(ls -1 "$USB4_PATH" | grep "$DOMAIN")
      [[ -n "$DEVICE_LIST" ]] || tst_brk TCONF "No domain under $USB4_PATH"
      tst_res TINFO "DOMAIN_LIST:$DOMAIN_LIST"
      ;;
    *)
      tst_brk TFAIL "Invalid device_type:$device_type"
      ;;
  esac
}

# Check sysfs file contain target content
# Input:
#   $1: sysfs file
#   $2: "xxx*xxx" //  if there is 2 use * to separate
#   $3: "$CONTAIN or $NULL"
# Output: return 0 for true, otherwise for false
check_file_contain() {
  local file=$1
  local keys=$2
  local key1=""
  local key2=""
  local key3=""
  local parm=$3
  local result=""


  key1=$(awk -F "*" '{print $1}' "$keys" 2>/dev/null)
  key2=$(awk -F "*" '{print $2}' "$keys" 2>/dev/null)
  key3=$(awk -F "*" '{print $3}' "$keys" 2>/dev/null)
  result=$(cat "$file" | grep -i "$key1" | grep -i "$key2" | grep -i "$key3")

  case $parm in
    "$CONTAIN")
      tst_res TINFO "keys:$keys should in $file info:$result"
      [[ -n "$result" ]] || tst_brk TFAIL "No $keys in dmesg"
      ;;
    "$NULL")
      tst_res TINFO "keys:$keys should not exist in $file info:$result"
      if [[ -z "$result" ]]; then
        tst_res TINFO "No $keys in $file:$result as expected."
      else
        tst_brk TFAIL "Should not contain $keys in $file:$result"
      fi
      ;;
    *)
      tst_brk TCONF "Invalid parm:$parm"
      ;;
  esac
}

# Check usb4 sysfs file
# Input:
#   $1: target folder
#   $1: sub folders under target folder
#   $2: check target file name
#   $3: check file content
# Output: return 0 for true, otherwise for false
check_usb4_sysfs() {
  local folder=$1
  local sub_folders=$2
  local target_file=$3
  local expect_content=$4
  local content=""
  local sub_folder=""
  local result=0

  for sub_folder in $sub_folders; do
    content=""
    content=$(cat ${folder}/${sub_folder}/${target_file} 2>/dev/null)
    if [[ "$content" == "$expect_content" ]]; then
      tst_res TINFO "Find ${folder}/${sub_folder}/${target_file}:$content"
      ((result++))
    fi
  done
  [[ "$result" -gt 0 ]] || return 1

  return 0
}

# Check is there usb4 device connected
# Input: none
# Output: return 0 for true, otherwise for false
is_usb4_device_connected() {
  is_usb4_sysfs || {
    tst_res TINFO "Platform doesn't contain usb4 controller sysfs"
    return 1
  }
  list_usb4_sysfolders "$DEVICE"
  check_usb4_sysfs "$USB4_PATH" "$DEVICE_LIST" "generation" "4" || {
    tst_res TINFO "Could not find usb4 device under $USB4_PATH"
    return 1
  }

  return 0
}

# Check is there only usb4 device connected
# Input: none
# Output: return 0 for true, otherwise for false
is_only_usb4_device_connected() {
  check_usb4_sysfs "$USB4_PATH" "$DEVICE_LIST" "generation" "3" && {
    tst_res TINFO "There is tbt3 device under $USB4_PATH"
    return 1
  }
  is_usb4_device_connected || return 1

  return 0
}

# Enable usb4 rtd3
# Input: none
# Output: return 0 for true, otherwise for false
init_rtd3() {
  local host=""

  list_usb4_sysfolders "$HOST"
  for host in $HOST_LIST; do
    tst_res TINFO "Set $AUTO into $USB4_PATH/$host/$POWER_CONTROL"
    do_cmd "echo $AUTO > $USB4_PATH/$host/$POWER_CONTROL"
  done
}

# Check usb4 rtd3 could work or not
# Input: none
# Output: return 0 for true, otherwise for false
rtd3_test() {
  local root_pci=""
  local rtd3_result=""

  [[ -n "$USED_DOMAIN" ]] || find_used_domain
  [[ -n  "$USED_DOMAIN" ]] || {
    tst_res TWARN "Could not find USED_DOMAIN:$USED_DOMAIN, set to 0"
    USED_DOMAIN="0"
  }
  root_pci=$(udevadm info --attribute-walk --path=${USB4_PATH}/${USED_DOMAIN}-0 \
            | grep "KERNEL" \
            | tail -n 2 \
            | grep -v "pci0000" \
            | cut -d "\"" -f 2)
  [[ -n "$root_pci" ]] || {
    tst_res TINFO "No usb4 root_pci:$root_pci find, will use 0-0 root pci"
    root_pci=$(udevadm info --attribute-walk --path=${USB4_PATH}/0-0 \
            | grep "KERNEL" \
            | tail -n 2 \
            | grep -v "pci0000" \
            | cut -d "\"" -f 2)
  }
  tst_res TINFO "Find usb4 ${USED_DOMAIN}-0 root pci:$root_pci and wait 25s to idle"
  sleep 25
  tst_res TINFO "Check ${PCI_DEVICES}/${root_pci}/${POWER_STATUS} for D3"
  rtd3_result=$(cat ${PCI_DEVICES}/${root_pci}/${POWER_STATUS})
  if [[ "$rtd3_result" == "$EXPECT_D3" ]]; then
    tst_res TINFO "Result status is D3:$rtd3_result"
    return 0
  else
    tst_res TINFO "Result status:$rtd3_result, not D3:$EXPECT_D3"
    return 1
  fi
}

# Prepare and test usb4 RTD3
# Input: none
# Output: return 0 for true, otherwise for false
usb4_rtd3_test() {
  find_used_domain
  init_rtd3
  rtd3_test || tst_brk TFAIL "rtd3 test failed"
}

# Check tbt device id
# Input: NA
# Return 0 for true, otherwise false or die
tbt_device_id() {
  local pcis=""
  local pci=""
  local is_tbt_pci=""
  local pci_device=""
  local tbt_device_num=0
  local pcih=""
  local pcil=""

  pcis=$(lspci | cut -d ' ' -f 1)
  for pci in $pcis; do
    is_tbt_pci=""
    pci_device=""
    pcih=""
    pcil=""
    is_tbt_pci=$(lspci -vv -s "$pci"| grep "$MOD_NAME")
    if [[ -z "$is_tbt_pci" ]]; then
      continue
    else
      TBT_PCIS="$TBT_PCIS $pci"
      tbt_device_num=$((tbt_device_num + 1))
      pcil=$(lspci -xx -s "$pci" | grep "^00: 86" | cut -d " " -f 4 2>/dev/null)
      pcih=$(lspci -xx -s "$pci" | grep "^00: 86" | cut -d " " -f 5 2>/dev/null)
      pci_device="${pcih}${pcil}"
      [[ -n "$pci_device" ]] \
        || tst_res TWARN "tbt device id is null->$(lspci | grep "$pci")"
      TBT_DEV_ID="$pci_device"
      TBT_DEV_IDS="$TBT_DEV_IDS $pci_device"
      tst_res TINFO "Found TBT/USB4 PCI:$pci, device_id:$pci_device!"
    fi
  done

  tst_res TINFO "TBT_PCIS:$TBT_PCIS"
  tst_res TINFO "TBT_DEV_IDS:$TBT_DEV_IDS"
  if [[ "$tbt_device_num" -eq 0 ]]; then
    tst_brk TFAIL "tbt_device_num is 0, need check dmesg, bios or PF is not supported tbt"
  fi
}

# check usb4 PCI device types
# Input: NA
# Return 0 for true, otherwise false or die
usb4_type_check() {
  local usb4_types="usb4_types"
  local tbt_pci=""
  local tbt_id_low=""
  local tbt_id_high=""
  local tbt_pci_id=""
  local usb4_pci_type=""
  local err_num=0

  [[ -n "$TBT_PCIS" ]] || tst_brk TFAIL "There was no TBT PCI in TBT_PCIS:$TBT_PCIS"
  usb4_types=$(which $usb4_types)
  for tbt_pci in $TBT_PCIS; do
    tbt_id_low=$(lspci -s $tbt_pci -xx | grep "00: " | cut -d ' ' -f 4)
    tbt_id_high=$(lspci -s $tbt_pci -xx | grep "00: " | cut -d ' ' -f 5)
    tbt_pci_id="${tbt_id_high}${tbt_id_low}"
    usb4_pci_type=$(cat "$usb4_types" | grep "$tbt_pci_id" | cut -d ' ' -f 1)
    [[ -n "$usb4_pci_type" ]] || {
      tst_res TWARN "tbt pci:$tbt_pci no usb4 type:$usb4_pci_type, maybe tbt3"
      err_num=$((err_num + 1))
    }
    tst_res TINFO "USB4 PCI:$tbt_pci, PCI ID:$tbt_pci_id, type:$usb4_pci_type"
  done

  if [[ "$err_num" -ne 0 ]]; then
    tst_brk TFAIL "USB4 type check error num is not 0:$err_num"
  fi
}

usb4v2_type_check() {
  local usb4_types="usb4_types"

  usb4_types=$(which $usb4_types)
  usb4_pci_type=$(cat "$usb4_types" | grep "$TBT_DEV_ID" | cut -d ' ' -f 1)
  [[ -z "$usb4_pci_type" ]] && skip_test "No USB4 type for dev id:$TBT_DEV_ID in $usb4_types"

  if [[ "$USB4V2_DEV_ID" == *"$TBT_DEV_ID"* ]]; then
    tst_res TINFO "Found USB4v2 type:$usb4_pci_type for dev id:$TBT_DEV_ID!"
    check_file_contain "$GENERATION_FILE" "$USB4_GEN" "$CONTAIN"
    check_file_contain "${USB4_PATH}/0-0/uevent" "USB4_VER*2.0" "$CONTAIN"
  else
    skip_test "It's USB4v1 not USB4v2:$usb4_pci_type for dev id:$TBT_DEV_ID, SKIP."
  fi
}

# find first usb4 device
# Input: NA
# Found usb4 device is USB4_DEVICE. Return 0 for true, otherwise false or die
find_usb4_device()
{
  local usb4_devices=""
  local usb4_device=""
  local gen=""

  usb4_devices=$(ls "$USB4_PATH" \
                  | grep "-" \
                  | grep -v ":" \
                  | grep -v "0$" \
                  | awk '{ print length(), $0 | "sort -n" }' \
                  | cut -d ' ' -f 2)
  for usb4_device in $usb4_devices; do
    gen=""
    gen=$(cat "$USB4_PATH"/"$usb4_device"/generation)
    if [[ "$gen" == "4" ]]; then
      tst_res TINFO "Found usb4 device: $usb4_device"
      USB4_DEVICE=$usb4_device
      break
    else
      continue
    fi
  done
}

# Restore the USB4 device firmware, firmware size should not be 0
# Input:
#   $1: USB4 device sysfs name
# Return 0 for true, otherwise false or die
restore_usb4_device_nvm()
{
  local usb4_device=$1
  local usb4_nvm_file="/tmp/usb4_device_nvm"
  local nvm_size=""

  $("dd if=${USB4_PATH}/${usb4_device}/${SNVM} of=${usb4_nvm_file}")
  nvm_size=$(ls -s "$usb4_nvm_file" | cut -d ' ' -f 1)
  if [[ nvm_size != "0" ]]; then
    tst_res TINFO "USB4 device:$usb4_device nvm size:$nvm_size not 0, pass."
  else
    tst_brk TFAIL "USB4 device:$usb4_device nvm size is 0, failed."
  fi
}

# Flash USB4 device firmware
# Input:
#   $1: USB4 NVM file
#   $2: GR device sysfs name
# Return 0 for true, otherwise false or die
flash_nvm()
{
  local nvm_file=$1
  local usb4_device=$2
  local flash_target=$3
  local verify_result=""

  [[ -e "$nvm_file" ]] || tst_brk TCONF "No $nvm_file to flash"
  # Check nvm_authenticate state before NVM flash
  verify_result=$(cat "${USB4_PATH}/${usb4_device}/$NVM_VERIFY")
  if [ "$verify_result" == "$VERIFY_SUCCESS" ]; then
    tst_res TINFO "NVM authenticate normal before flash:$verify_result"
  else
    tst_brk TCONF "NVM authenticate is not '0x0' before nvm:$verify_result"
  fi

  $("dd if=$nvm_file of=$flash_target")
  sleep 2
  $("echo 1 > ${USB4_PATH}/${usb4_device}/$NVM_VERIFY")
  sleep 80
  verify_result=$(cat ${USB4_PATH}/${usb4_device}/$NVM_VERIFY)
  [[ "$verify_result" == "$VERIFY_SUCCESS" ]] || sleep 120
  verify_result=$(cat ${USB4_PATH}/${usb4_device}/$NVM_VERIFY)
  if [ "$verify_result" == "$VERIFY_SUCCESS" ]; then
    tst_res TINFO "Flash $usb4_device NVM $nvm_file successfully!"
  else
    tst_brk TFAIL "$nvm_file fail, ${USB4_PATH}/${usb4_device}/$NVM_VERIFY:$verify_result"
  fi
}

# Check dmesg info contain matched content
# Input: $1 dmesg log path
#        $2 search content
#        $3 fail/pass: fail should not contain key word, pass should contain
# Return: 0 for not find, 1 for find and give the warning print
dmesg_check() {
  local dmesg_file="$1"
  local content="$2"
  local verify="$3"
  local result=""

  [[ -e "$dmesg_file" ]] || tst_brk TFAIL "file $dmesg_file is not exist"

  result=$(grep -i "$content" "$dmesg_file")

  if [[ "$verify" == "$FAIL" ]]; then
    if [[ -z "$result" ]]; then
      tst_res TINFO "No $content find when this case test"
    else
      tst_res TWARN "Find $content in dmesg:$result"
      tst_brk TFAIL "Find $content in dmesg $dmesg_file."
    fi
  elif [[ "$verify" == "$PASS" ]]; then
    if [[ -z "$result" ]]; then
      tst_brk TFAIL "Not find $content in dmesg $dmesg_file."
    else
      tst_res TPASS "Find $content in dmesg:$result"
    fi
  else
    tst_brk TCONF "Invalid verify content:$verify"
  fi
}

# This function perform write test on usb device
# Input:
#       $1: device to be performed test on
#       $2: data source
#       $3: bs block size
#       $4: bc block count
# Return: Write with dd command, 0 for true, otherwise false
write_test_with_file() {
  local of=$1
  local if=$2
  local bs=$3
  local bc=$4
  local if_size=""
  if_size=$(du -b "$if" | awk '{print $1}')
  tst_res TINFO "size of $if is $if_size"
  if [[ -z "$bc" ]]; then
    $(dd if="$if" of="$of" bs="$if_size" count=1)
  else
    $(dd if="$if" of="$of")
  fi
  return $?
}

# This function perform read test on usb device
# Input:
#       $1: device to be performed test on
#       $2: original file, to be compared with the read out data
#       $3: bs, block size
#       $4: bc, block count
# Return: Read with dd command, 0 for true, otherwise false
read_test_with_file() {
  local if=$1
  local test_file=$2
  local bs=$3
  local bc=$4
  local of="${test_file}.copy"
  local file_size=""

  file_size=$(du -b "$test_file" | awk '{print $1}')
  tst_res TINFO "size of $test_file is $file_size"
  if [[ -z "$bc" ]]; then
    $(dd if="$if" of="$of" bs="$file_size" count=1)
  else
    $(dd if="$if" of="$of")
  fi
  tst_res TINFO "Check diff for files: $test_file and $of"
  $(diff "$test_file" "$of")
  if [ $? -eq 0 ]; then
    tst_res TPASS "diff test pass"
    return 0
  else
    tst_brk TFAIL "diff $test_file $of test failed"
  fi
}

# Set 1 for authorized file
enable_authorize_file() {
  local authorize_file=$1
  local authorize_info=""
  if [ -e "$authorize_file" ]; then
    authorize_info=$(cat "$authorize_file")
    if [[ "$authorize_info" -eq 0 ]]; then
      tst_res TINFO  "Change 0 to 1: $authorize_file"
      eval "echo 1 > $authorize_file" 2>/dev/null
      sleep 5
    else
      tst_res TPASS "$authorize_file: $authorize_info"
    fi
  fi
}

# Check TBT authorized file and try to set 1
# No input
# Result: 0 for true, otherwise false
enable_authorized() {
  local aim_folders=""
  local authorize_info=""
  local check_result=""
  local aim_folder=""
  aim_folders=$(ls -1 ${TBT_PATH} \
              | grep "$REGEX_ITEM" \
              | grep -v ":" \
              | awk '{ print length(), $0 | "sort -n" }' \
              | cut -d ' ' -f 2)
  [[ -n "$aim_folders" ]] || tst_brk TCONF "Device folder is not exist"
  # Should set authorized to 1 less than 10 round
  for ((i = 1; i <= 10; i++)); do
    check_result=$(cat ${AUTHOR_PATH} | grep 0)
    if [[ -z "$check_result" ]]; then
      tst_res TPASS "All authorized set to 1"
      break
    else
      tst_res TINFO "$i round set 1 to authorized:"
      for aim_folder in ${aim_folders}; do
        enable_authorize_file "${TBT_PATH}/${aim_folder}/${AUTHORIZE_FILE}"
      done
    fi
  done
  if [[ "$i" -ge 10 ]]; then
    tst_brk TCONF "Set 1 to authorized with 10 round, should not reach 10 round. i:$i"
  fi
  # Avoid fake failure, wait 3s to check all device should be recognized next
  sleep 3
}

# This function check need used space should smaller than free space
# Input:
#       $1: free space size
#       $2: block size of the file, with unit
#       $3: block count
# Return:
#       0: needed size < free space, could test read and write
#       1: needed size > free space and block test
check_free_space() {
  local free_size=$1
  local block_size=$2
  local block_count=$3
  local need_size=""
  local free_bytes_size=""
  local double_count=""
  double_count=$(( block_count * 2 ))

  SPACE_RESULT=""
  need_size=$(caculate_size_in_bytes "$block_size" "$double_count")
  free_bytes_size=$(caculate_size_in_bytes "$free_size" 1)
  tst_res TINFO "Needed size:$need_size, free space size:$free_bytes_size"
  if [[ "$need_size" -ge "$free_bytes_size" ]]; then
    tst_res TWARN "No enough free space to test read/write!"
    SPACE_RESULT=1
  fi
  SPACE_RESULT=0
}

serial_cmd() {
  local command=$1
  local cmd_file=""
  local escape="escape.txt"
  local esc_file=""
  local usb4switch_log="/tmp/capture.txt"


  esc_file=$(which $escape 2>/dev/null)
  cmd_file=$(which $command 2>/dev/null)
  tst_res TINFO "escape:$esc_file, cmd_file:$cmd_file"
  {
    sleep 1
    cat $esc_file
  } | minicom -b 9600 -D /dev/ttyACM0 -S $cmd_file -C $usb4switch_log
}


usb4switch_plugin() {
  local plug_state=""

  plug_state=$(serial_cmd "status" | grep "PORTF: 0x12" 2>/dev/null)
  if [[ -n "$plug_state" ]]; then
    tst_res TINFO "Already connected port 1 for USB4 switch:$plug_state"
  else
    tst_res TINFO "plug_state:$plug_state not 0x12, will connect port1"
    serial_cmd "superspeed"
    serial_cmd "port1"
  fi
}

usb4switch_plugout() {
  local plug_state=""

  plug_state=$(serial_cmd "status" | grep "PORTF: 0x70" 2>/dev/null)
  if [[ -n "$plug_state" ]]; then
    tst_res TINFO "Already disconnected port 1 for USB4 switch:$plug_state"
  else
    tst_res TINFO "plug_state:$plug_state not 0x70, will disconnect."
    serial_cmd "superspeed"
    serial_cmd "port0"
  fi
}

# Thunderbolt devices plug in by cleware power on tbt
# No input
# Return: 0 for true, otherwise false or die
plug_in_tbt() {
  boltctl forget -a

  is_exist_any_cswitch_device
  is_exist_any_cswitch_device_return_code=$?

  if [[ "$is_exist_any_cswitch_device_return_code" -eq 0 ]]; then
    tst_res TINFO  "Plug in tbt by $CSWITCH_CONTROL_DEVICE"
    #Set default port 1.
    cswitchControl_change_port 1
    tst_res TINFO "Wait $DEFAULT_SLEEP_TIME seconds for stabilization"
    sleep "$DEFAULT_SLEEP_TIME"
  elif [[ -n "$HID_CONTENT" ]]; then
    # plug in thunderbolt devices action
    tst_res TINFO "Plug in tbt by cleware power on......"
    $("cleware 1")
    # need time to wait all thunderbolt devices ready to do next step
    tst_res TINFO "sleep 30"
    sleep 30
  elif [[ -e "$ACM0" ]]; then
    local state=""
    state=$(serial_cmd "status" | grep "PORTF" 2>/dev/null)
    if [[ -z "$state" ]]; then
      CLE=1
      tst_res TWARN "No USB4 switch 3141 or cleware:$state"
      return 1
    else
      tst_res TINFO "Used USB4 switch 3141 tool, state:$state"
    fi
    usb4switch_plugin
    # need time to wait all thunderbolt devices ready to do next step
    tst_res TINFO "sleep 30"
    sleep 30
  else
    CLE=1
    tst_res TWARN "No Cleware:$HID_CONTENT or USB4 switch:$ACM0 for plug in"
  fi
}

# Thunderbolt devices plug out function
# No input
# Return: 0 for true, otherwise false or die
plug_out_tbt() {
  local bolt_info=""
  local bolt=""
  local bolt_path="/var/lib/boltd/devices"

  is_exist_any_cswitch_device
  is_exist_any_cswitch_device_return_code=$?

  if [[ "$is_exist_any_cswitch_device_return_code" -eq 0 ]]; then
    test_print_trc "Plug out tbt by $CSWITCH_CONTROL_DEVICE"
    cswitchControl_change_port 0
    test_print_trc "Wait $DEFAULT_SLEEP_TIME seconds for stabilization"
    sleep "$DEFAULT_SLEEP_TIME"
  elif [[ -n "$HID_CONTENT" ]]; then
    # plug out thunderbolt devices action
    $("cleware 0")
  elif [[ -e "$ACM0" ]]; then
    local state=""
    state=$(serial_cmd "status" | grep "PORTF" 2>/dev/null)
    if [[ -z "$state" ]]; then
      CLE=1
      tst_res TWARN "No USB4 switch or cleware:$state for plug out"
      return 1
    else
      tst_res TINFO "Used USB4 switch 3141 tool, state:$state"
    fi
    usb4switch_plugout
  else
    tst_res TWARN "No Cleware:$HID_CONTENT or USB4 switch:$ACM0 for plug out"
    CLE=1
  fi

  # ubuntu 18.04 add auto authorized function, which impact tbt authorize test
  sleep 2
  bolt_info=$(boltctl 2>/dev/null | grep uuid | awk -F "uuid:" '{print $2}')
  for bolt in $bolt_info; do
    if [[ -e "${bolt_path}/${bolt}" ]]; then
      # due to boltctl is not supported in old linux os, do_cmd will not use
      tst_res TINFO "Stop auto authorize tbt $bolt"
      boltctl forget "$bolt" 2>/dev/null
    fi
  done
  boltctl forget -a
}

# Security file should be exist and show its content
check_security_mode() {
  local domain="domain${DOMAINX}/security"

  if [[ -e "${TBT_PATH}/${domain}" ]]; then
    SECURITY=$(cat ${TBT_PATH}/${domain})
  else
    tst_brk TFAIL "${TBT_PATH}/${domain} does not exist"
  fi
  [[ -n "$SECURITY" ]] || tst_brk TCONF "SECURITY:$SECURITY is null."
}

# check security status, if no security status, will try power on cleware
# No input
# Return: 0 for true, otherwise false or die
check_auto_connect() {
  local tbt=""
  local result=""

  if [[ -e "${TBT_PATH}/domain0/security" ]]; then
    check_security_mode
    tst_res TINFO "It's $SECURITY mode"
  else
    tst_res TINFO "Could not detect tbt mode"
  fi

  # due to multi domains, need exclude all domains X-0
  tbt=$(ls "$TBT_PATH" | grep "$REGEX_ITEM" | grep -v "$HOST_EXCLUDE")

  is_exist_any_cswitch_device
  is_exist_any_cswitch_device_return_code=$?

  if [[ -n "$tbt" ]]; then
    tst_res TINFO "Found tbt device"
  elif [[ "$is_exist_any_cswitch_device_return_code" -eq 0 ]]; then
    tst_res TINFO "Found connected $CSWITCH_CONTROL_DEVICE."
    plug_in_tbt
  elif [[ -z "$HID_CONTENT" ]]; then
    tst_res TINFO "No cleware or Cswitch connect to tbt device!"
    tst_res TINFO "Power off/on tbt device test could not test correctly..."
  else
    result=$(cleware s | grep status | head -n 1 | awk -F "=" '{print $NF}')
    if [[ "$result" -eq 1 ]]; then
      tst_res TINFO "Cleware is already connected:$result"
    else
      plug_in_tbt
    fi
  fi
}

# Check necessary command workable
check_test_env() {
    which du &> /dev/null
    [[ $? -eq 0  ]] || tst_brk TCONF "du doesn't exist in current environment!"
    which dd &> /dev/null
    [[ $? -eq 0  ]] || tst_brk TCONF "dd doesn't exist in current environment!"
    which diff &> /dev/null
    [[ $? -eq 0  ]] || tst_brk TCONF "diff doesn't exist in current environment!"
    which fdisk &> /dev/null
    [[ $? -eq 0  ]] || tst_brk TCONF "fdisk doesn't exist in current environment!"
}

# Check /tmp contain enough space, othewise will check /root free space
# Input:
#  $1: block size of the file
#  $2: block count
# Return:
#  0: find enough free space partition and create random temporary folder
#     otherwise block test due to no enough partition find
check_free_partition() {
  local block_size=$1
  local block_cnt=$2
  local tmp_folder=""

  check_free_space "$FREE_SPACE" "$block_size" "$block_cnt"
  if [[ "$SPACE_RESULT" -eq 0 ]]; then
    tst_res TINFO "/tmp has enough space to test read/write file"
    TEMP_DIR=$(mktemp -d)
  else
    tst_res TINFO "/tmp no enough space, will have a try in /root"
    check_free_space "$FREE_SPACE_ROOT" "$block_size" "$block_cnt"
    [[ "$SPACE_RESULT" -eq 0 ]] || tst_brk TCONF "no enought space in /root"
    tmp_folder=$(cat /dev/urandom | head -n 10 | md5sum | head -c 10)
    TEMP_DIR="/root/${tmp_folder}"
    if [[ -d "$TEMP_DIR" ]]; then
      tst_res TWARN "$TEMP_DIR already exist"
    else
      do_cmd "rm -rf $TEMP_DIR"
      do_cmd "mkdir -p $TEMP_DIR"
    fi
  fi
}

# Check all proper type usb node saved in DEVICES
# Input:
# $1: node, $2: speed
# Return 0 for true, otherwise false or block test
protocol_check() {
  local dev_node=$1
  local speed=$2
  local speed_count=0
  local controller_count=0
  local j=0

  controller_count=$(udevadm info --attribute-walk --name=/dev/"$dev_node" \
                     | grep -C 5 speed \
                     | grep -C 5 "$speed" \
                     | awk -v RS="@#$j" '{print gsub(/Controller/,"&")}')

  speed_count=$(udevadm info --attribute-walk --name=/dev/"$dev_node" \
                | grep -C 5 speed \
                | grep -C 5 "$speed" \
                | awk -v RS="@#$j" '{print gsub(/speed/,"&")}')

  if [[ "$speed_count" -gt "$controller_count" ]]; then
    tst_res TINFO "Find $dev_node for speed $speed"
  else
    return 1
  fi
}

# Detect requested device type node
# Input:
# $1: node,  $2: device type like uas or flash
# Return 0 for true, otherwise false or block test
device_check() {
  local dev_node=$1
  local type=$2
  local driver=""

  case $type in
    flash) driver="usb-storage" ;;
    uas) driver="uas" ;;
    *) tst_brk TCONF "bad device type:$type" ;;
  esac

  udevadm info --attribute-walk --name=/dev/"$dev_node" \
                                | grep DRIVERS \
                                | grep -q $driver
  return $?
}

# This function find all nodes for matched speed
# Input:
# $1: speed like 480 5000
# $2: device type like flash or uas
# Return 0 for true, otherwise false or block test
protocol_type() {
  local speed=$1
  local device_tp=$2
  local device_nodes=""
  local node=""

  device_nodes=$(ls /dev/sd[a-z] | awk -F '/' '{print $NF}')
  [[ -n "$device_nodes" ]] || tst_brk TCONF "No nodes find:$device_nodes"

  for node in $device_nodes
  do
    protocol_check "$node" "$speed" || continue
    device_check "$node" "$device_tp" || continue
    DEVICES="$DEVICES $node"
  done
}

# Integrated TBT root PCI will be changed so need to verify and find it
# Input: $1 TBT_ROOT_PCI
# Return 0 for true, otherwise false or block_test
verify_tbt_root_pci() {
  local root_pci=$1
  local dev_path="/sys/devices/pci0000:00"
  local result=""
  local pf=""
  local pf_name=""
  local tbt_dev=""

  tbt_dev=$(ls ${TBT_PATH} \
              | grep "$REGEX_ITEM" \
              | grep -v "$HOST_EXCLUDE" \
              | awk '{ print length(), $0 | "sort -n" }' \
              | cut -d ' ' -f 2 \
              | head -n1)

  pf=$(dmidecode --type bios \
            | grep Version \
            | cut -d ':' -f 2)
  pf_name=$(echo ${pf: 1: 4})

  result=$(ls -1 $dev_path/$root_pci | grep "0000" | grep "07")
  if [[ -z "$result" ]]; then
    [[ "$tbt_dev" == *"-1"* ]] && {
      [[ "$root_pci" == *"0d.2" ]] && TBT_ROOT_PCI="0000:00:07.0"
      [[ "$root_pci" == *"0d.3" ]] && TBT_ROOT_PCI="0000:00:07.2"
    }
    [[ "$tbt_dev" == *"-3"* ]] && {
      [[ "$root_pci" == *"0d.2" ]] && TBT_ROOT_PCI="0000:00:07.1"
      [[ "$root_pci" == *"0d.3" ]] && TBT_ROOT_PCI="0000:00:07.3"
    }
    SYS_PATH="$dev_path/$TBT_ROOT_PCI"
    tst_res TINFO "Discrete or FW CM on $pf_name,root:$TBT_ROOT_PCI, $SYS_PATH"
  elif [[ "$tbt_dev" == *"-1"* ]]; then
    # SW CM
    TBT_ROOT_PCI=$(ls -1 $dev_path/$root_pci \
                  | grep "0000" \
                  | grep "07" \
                  | head -n 2 \
                  | head -n 1 \
                  | awk -F "pci:" '{print $2}')
    # FW CM
    [[ -z "$TBT_ROOT_PCI" ]] && {
      TBT_ROOT_PCI=$(ls -1 $dev_path/$root_pci \
                    | grep "0000" \
                    | grep "07" \
                    | head -n 2 \
                    | head -n 1 \
                    | awk -F ":pci" '{print $1}')
    }
    SYS_PATH="$dev_path/$TBT_ROOT_PCI"
    tst_res TINFO "Integrated on $pf_name, $tbt_dev $root_pci -> $TBT_ROOT_PCI"
  elif [[ "$tbt_dev" == *"-3"* ]]; then
    # SW CM
    TBT_ROOT_PCI=$(ls -1 $dev_path/$root_pci \
                  | grep "0000" \
                  | grep "07" \
                  | head -n 2 \
                  | tail -n 1\
                  | awk -F "pci:" '{print $2}')
    # FW CM
    [[ -z "$TBT_ROOT_PCI" ]] && {
      TBT_ROOT_PCI=$(ls -1 $dev_path/$root_pci \
                    | grep "0000" \
                    | grep "07" \
                    | head -n 2 \
                    | head -n 1 \
                    | awk -F ":pci" '{print $1}')
    }
    SYS_PATH="$dev_path/$TBT_ROOT_PCI"
    tst_res TINFO "Integrated on $pf_name, $tbt_dev $root_pci -> $TBT_ROOT_PCI"
  elif [[ -z "$tbt_dev" ]]; then
    [[ "$root_pci" == *"0d.2" ]] && TBT_ROOT_PCI="0000:00:07.0"
    [[ "$root_pci" == *"0d.3" ]] && TBT_ROOT_PCI="0000:00:07.2"
    SYS_PATH="$dev_path/$TBT_ROOT_PCI"
  else
    tst_brk TCONF "Invalid tbt device sysfs:$tbt_dev"
  fi
  tst_res TINFO "TBT ROOT:$TBT_ROOT_PCI SYS_PATH:$dev_path/$TBT_ROOT_PCI"
}

# Check new fail info in the dmesg log, and print fail info
# Input: none
# Return: Print warning and fail info to check whether it's an issue
fail_dmesg_check() {
  local dmesg_path=""
  local hidden="partially hidden"
  local bridge_unusable="devices behind bridge are unusable"
  local call_trace="Call Trace"

  FAIL_FINAL=$(dmesg | grep -i $MOD_NAME | grep -i $FAIL)
  if [[ "$FAIL_ORIGIN" == "$FAIL_FINAL" ]]; then
    tst_res TINFO "check dmesg log pass, no fail info in last case test"
  else
    tst_res TWARN "Found new fail info, fail_origin:$FAIL_ORIGIN"
    tst_res TWARN "fail_final:$FAIL_FINAL"
    FAIL_ORIGIN=$FAIL_FINAL
  fi

  #dmesg_path=$(extract_case_dmesg -f)
  #[[ -e "$LOG_PATH/$dmesg_path" ]] || {
  #  tst_res TWARN "No case dmesg:$LOG_PATH/$dmesg_path exist"
  #  return 1
  #}
  #dmesg_check "$LOG_PATH/$dmesg_path" "$hidden" "$FAIL"
  #dmesg_check "$LOG_PATH/$dmesg_path" "$bridge_unusable" "$FAIL"
  #dmesg_check "$LOG_PATH/$dmesg_path" "$call_trace" "$FAIL"
}

# mound device node to target folder
# otherwise faile the case, and gave the average gap percentage
# Input: $1 device node like /dev/sdc
#        $2 mount target
# Return 0 for true, otherwise false or die
mount_dev() {
  local dev_name=$1
  local mount_folder=$2
  local mount_result=""
  local node=""

  [[ -n "$mount_folder" ]] || {
    tst_res TINFO "no mount_folder:$mount_folder set to $MOUNT_FOLDER"
    mount_folder=$mount_folder
  }
  [[ -n "$dev_name" ]] || {
    tst_res TINFO "no dev_name:$dev_name set to $DEVICE_NODE"
    dev_name=$DEVICE_NODE
  }

  [[ -d "$mount_folder" ]] || {
    tst_res TINFO "mount_folder $mount_folder is not exist, create it"
    $("rm -rf $mount_folder")
    $("mkdir -p $mount_folder")
  }

  node=$(echo "$dev_name" | awk -F '/' '{printf $NF}')
  [[ -n "$node" ]] || tst_brk TCONF "node is null:$node, dev_name:$dev_name"
  mount_result=$(lsblk | grep "$node" | grep "$mount_folder" 2>/dev/null)
  if [[ -n "$mount_result" ]]; then
    tst_res TINFO "node $node in $dev_name already mount to $mount_folder"
  else
    umount -f $mount_folder 2>/dev/null
    tst_res TINFO "mount $dev_name $mount_folder"
    mount $dev_name $mount_folder
    [[ $? -eq 0 ]] || {
      tst_res TINFO "Bad superblock on $dev_name, will format and remount"
      $("mkfs.ext4 -F $dev_name")
      $("mount $dev_name $mount_folder")
    }
  fi
  tst_res TINFO "$DEVICE_NODE"
  $("lsblk")
}

# This function generate request size test file to read and write
# Input:
#       $1: block size of the file, with unit
#       $2: block count of the file
#       $3: where the file will be created
# Output:
#       Return path of the created file on success, fail return null
generate_test_file() {
  local block_size=$1
  local block_count=$2
  local dir=$3
  local if="/dev/random"
  local of=""
  of="${dir}/test.$(date +%Y%m%d%H%M%S)"
  $(dd if="$if" of="$of" bs="$block_size" count="$block_count")
  if [ $? -eq 0 ] && [ -e "$of" ]; then
    echo "$of"
  else
    echo
  fi
}

# get specific sys path for ICL/TGL
# Input: NA
# Return 0 for true, otherwise false or block test
get_specific_sys_path() {
  local tbt_dev=""

  tbt_dev=$(ls ${TBT_PATH} \
              | grep "$REGEX_ITEM" \
              | grep -v "$HOST_EXCLUDE" \
              | awk '{ print length(), $0 | "sort -n" }' \
              | cut -d ' ' -f 2 \
              | head -n1)

  case ${tbt_dev} in
    0-1)
      SYS_PATH="$dev_path/0000:00:07.0"
      ;;
    0-3)
      SYS_PATH="$dev_path/0000:00:07.1"
      ;;
    1-1)
      SYS_PATH="$dev_path/0000:00:07.2"
      ;;
    1-3)
      SYS_PATH="$dev_path/0000:00:07.3"
      ;;
    *)
      tst_brk TCONF "Invalid tbt_dev:$tbt_dev, maybe tbt device not connect"
      ;;
  esac
}

# Get the tbt root port pci and save it in TBT_PCI
# Input: null
# Return 0 for true, otherwise false or block_test
get_tbt_pci() {
  local domain_file=""
  local dev_path="/sys/devices/pci0000:00"
  local pf_name=""

  pf_name=$(dmidecode --type bios \
            | grep Version \
            | cut -d ':' -f 2)

  # due to ICL/TGL design change, hard code for different root port pci
  case $pf_name in
    *ICL*)
      tst_res TINFO "ICL platform"
      check_auto_connect
      get_specific_sys_path
      ;;
    *TGL*)
      tst_res TINFO "TGL platform"
      check_auto_connect
      get_specific_sys_path
      ;;
    *)
      domain_file=$(ls ${TBT_PATH} | grep ${REGEX_DOMAIN}${DOMAINX} | head -n 1)
      SYS_PATH="${TBT_PATH}/${domain_file}"
      ;;
  esac

  TBT_ROOT_PCI=$(udevadm info --attribute-walk --path=${SYS_PATH} \
                | grep "KERNEL" \
                | tail -n 2 \
                | grep -v pci0000 \
                | cut -d "\"" -f 2)
  verify_tbt_root_pci "$TBT_ROOT_PCI"

  TBT_PCI=$(udevadm info --attribute-walk --path=${SYS_PATH} \
            | grep "KERNEL" \
            | tail -n 2 \
            | awk -F '==' '{print $NF}')

  [[ -n "$TBT_PCI" ]] || tst_brk TCONF "TBT_PCI is null:$TBT_PCI"
  tst_res TINFO "tbt top 2 pci path:$TBT_PCI"
}

# Check node size should be bigger than read/write block need size
# Return 0 for true, otherwise false
check_test_size() {
  local device_node=$1
  local block_size=$2
  local block_count=$3
  local needed_size=""
  local actual_size=""
  actual_size=$(fdisk -l "$device_node" 2> /dev/null \
              | grep "$device_node" \
              | grep bytes \
              | awk '{print $5}')
  needed_size=$(caculate_size_in_bytes "$block_size" "$block_count")

  # Due to large file tests, if free space smaller than request,
  # use all free space to test and just warn it and didn't return 1
  [[ "$actual_size" -gt "$needed_size" ]] || {
    # actual_size is caculated as bytes, possible round-off, so -10 bytes
    actual_size=$((actual_size - 10))
    tst_res TWARN "$device_node free < $needed_size, free-10:$actual_size Bytes"
    BLOCK_SIZE=$actual_size
    BLOCK_COUNT=1
  }
}

# Detect the DEVICE connected by thunderbolt and not system disk
# Return 0 if true, otherwise false
check_tbt_connect() {
  local dev_node=$1
  local pci_tbt=$2
  local pci_dev=""
  local sys_disk=""
  tst_res TINFO "Check the device node: $dev_node"
  # Return 2 if system disk with this device
  sys_disk=$(df -Ph /boot | tail -1 | awk '{print $1}')
  if [[ "$sys_disk" == *"$dev_node"* ]]; then
    tst_res TINFO "Node $dev_node is system disk, will not test on it!"
    return 2
  fi
  # Check device connected with thunderbolt, return non-zero if not
  # due to ICL desgin changed, just check top level 2 pci match is enough
  pci_dev=$(udevadm info --attribute-walk --name=/dev/"$dev_node" \
          | grep "KERNEL" \
          | tail -n 2 \
          | awk -F '==' '{print $NF}')
  if [[ "$pci_tbt" == "$pci_dev" ]]; then
    tst_res TINFO "tbt pci path the same as $dev_node pci path"
    return 0
  # USB2.0 under USB4 device will use this 1d.0 PCI
  elif [[ "$pci_dev" == *"0d.0"* ]]; then
    tst_res TINFO "tbt pci:$pci_tbt"
    tst_res TINFO "device pci path is 0d.0 usb common:$pci_dev"
    return 0
  # USB3.0 under USB4 device will use this 14.0 PCI
  elif [[ "$pci_dev" == *"14.0"* ]]; then
    tst_res TINFO "tbt pci:$pci_tbt"
    tst_res TINFO "device pci path is 14.0 usb commmon:$pci_dev"
    return 0
  else
    tst_res TINFO "device not connect by thunderbolt, tbt pci path:$pci_tbt"
    tst_res TINFO "device pci path:$pci_dev"
    return 1
  fi
}

# This function detect the device node connected by tbt
# Input:
# $1: block size
# $2: block count
# $3: protocol type like ssd, 2.0, 3.0, 3.1
# $4: device type like flash, uas
# Return 0 for true, otherwise false or block_test
find_tbt_device() {
  local block_size=$1
  local block_count=$2
  local protocol=$3
  local device_tp=$4
  local speed=""
  local device=""
  local dev_node=""
  local device_size=""
  local dmesg_log=""

  # clean old DEVICE_NODE and NODE if one case use this function twice
  DEVICE_NODE=""
  NODE=""

  get_tbt_pci

  case $protocol in
    2.0)
      speed="480"
      protocol_type "$speed" "$device_tp"
      ;;
    3.0)
      speed="5000"
      protocol_type "$speed" "$device_tp"
      ;;
    3.1)
      speed="10000"
      protocol_type "$speed" "$device_tp"
      ;;
    ssd)
      # Detect all SSD node
      DEVICES=$(lsblk -d -o name,rota | grep sd | grep 0 | awk '{print $1}')
      ;;
    nvme)
      # Detect all nvme node
      DEVICES=$(lsblk -d -o name,rota | grep nvme | grep 0 | awk '{print $1}')
      ;;
    *)
      tst_brk TCONF "Bad device type:$protocol $device_tp"
      ;;
  esac

  if [[ -z "$DEVICES" ]]; then
    dmesg_log=$(extract_case_dmesg -f)
    tst_brk TCONF "No nodes for $protocol $device_tp, dmesg:$LOG_PATH/$dmesg_log"
  else
    tst_res TINFO "Find nodes for $protocol $device_tp:"
    tst_res TINFO "$DEVICES"
  fi
  # Detect devices connected by thunderbolt
  for device in ${DEVICES}; do
    check_tbt_connect "$device" "$TBT_PCI" || continue
    check_test_size "/dev/$device" "$block_size" "$block_count" || continue
    device_size=$(fdisk -l "/dev/$device" 2> /dev/null \
             | grep "/dev/$device" \
             | grep bytes \
             | awk '{print $5}')
    tst_res TINFO "Find tbt device $device, size: $device_size"
    dev_node="$device"
    break
  done
  [ -n "$dev_node" ] || tst_brk TCONF "Not find $protocol $device_tp connect with tbt"
  tst_res TINFO "Find $protocol $device_tp connect with tbt: $dev_node"
  NODE="$dev_node"
  DEVICE_NODE="/dev/$dev_node"
  return 0
}

# check is there tbt device connected
# No input
# Return: 0 for true, otherwise for die
no_tbt_device_check() {
  local tbt_sysfs=""

  # wait tbt driver to clean all tbt devices
  sleep 15
  tbt_sysfs=$(ls $USB4_PATH | grep "-" | grep -v "0$")
  if [[ -z "$tbt_sysfs" ]]; then
    tst_res TINFO "No tbt device connected as expected"
  else
    tst_brk TCONF "There was tbt connected unexpectedly after plugged out 15s:$tbt_sysfs"
  fi
}

# Execute suspend test
# Input $1: suspend type like freeze, deep, disk
# Output: 0 for true, otherwise false or die
suspend_test() {
  local suspend_type=$1
  local rtc_time=20
  local disk_time=50
  local mem="mem"

  # Clear Linux no /sys/power/disk and pm_test, add the judgement
  if [[ -e "$POWER_DISK_NODE" ]]; then
    $("echo platform > '$POWER_DISK_NODE'")
  else
    tst_res TINFO "No file $POWER_DISK_NODE exist"
  fi
  if [[ -e "$POWER_PM_TEST_NODE" ]]; then
    $("echo none > '$POWER_PM_TEST_NODE'")
  else
    tst_res TINFO "No file $POWER_PM_TEST_NODE exist"
  fi

  case $suspend_type in
    freeze)
      tst_res TINFO "rtcwake -m $suspend_type -s $rtc_time"
      rtcwake -m $suspend_type -s $rtc_time
      [[ "$?" -eq 0 ]] || tst_brk TCONF "fail to resume from $suspend_type!"
      sleep 10
      ;;
    s2idle)
      tst_res TINFO "set $suspend_type in $POWER_MEM_SLEEP"
      echo "$suspend_type" > $POWER_MEM_SLEEP
      tst_res TINFO "rtcwake -m $mem -s $rtc_time"
      rtcwake -m "$mem" -s "$rtc_time"
      [[ "$?" -eq 0 ]] || tst_brk TCONF "fail to resume from $suspend_type!"
      ;;
    deep)
      tst_res TINFO "set $suspend_type in $POWER_MEM_SLEEP"
      echo "$suspend_type" > $POWER_MEM_SLEEP
      tst_res TINFO "rtcwake -m $mem -s $rtc_time"
      rtcwake -m "$mem" -s "$rtc_time"
      [[ "$?" -eq 0 ]] || tst_brk TCONF "fail to resume from $suspend_type!"
      ;;
    disk)
      tst_res TINFO "rtcwake -m $suspend_type -s $disk_time"
      rtcwake -m "$suspend_type" -s "$disk_time"
      [[ "$?" -eq 0 ]] || tst_brk TCONF "fail to resume from $suspend_type!"
      ;;
    *)
      tst_brk TCONF "suspend_type: $suspend_type not support"
      ;;
  esac
  do_cmd "sleep 10"
}

# Check usb4 device exist after suspend test, all device could display normally
# Input: $1 suspend type
# Output: 0 for true, otherwise false or die
usb4_suspend_test() {
  local suspend_type=$1
  local origin_sysfs=""
  local result_sysfs=""
  local origin_pci=""
  local result_pci=""

  origin_pci=$(lspci -t)
  origin_sysfs=$(ls "$USB4_PATH" \
                  | grep "-" \
                  | grep -v ":" \
                  | grep -v "0$" \
                  | awk '{ print length(), $0 | "sort -n" }' \
                  | cut -d ' ' -f 2)
  suspend_test "$suspend_type"
  # Wait usb4 devices resume in sleep
  $("sleep 20")
  result_pci=$(lspci -t)
  result_sysfs=$(ls "$USB4_PATH" \
                  | grep "-" \
                  | grep -v ":" \
                  | grep -v "0$" \
                  | awk '{ print length(), $0 | "sort -n" }' \
                  | cut -d ' ' -f 2)
  if [[ "$origin_sysfs" == "$result_sysfs" ]]; then
    tst_res TINFO "$suspend_type passed, origin:$origin_sysfs"
    tst_res TINFO "After resume, result:$result_sysfs"
  else
    tst_brk TCONF "$suspend_type failed, origin:$origin_sysfs, result:$result_sysfs"
  fi

  if [[ "$origin_pci" == "$result_pci" ]]; then
    tst_res TINFO "After $suspend_type, origin and result pci same:$origin_pci"
  else
    tst_brk TCONF "$suspend_type failed, origin pci:$origin_pci, result pci:$result_pci"
  fi
}

check_usb4_version() {
  if [[ -e "${USB4_PATH}/0-0/uevent" ]]; then
    USB4_VER=$(grep "USB4_VERSION" ${USB4_PATH}/0-0/uevent | cut -d "=" -f 2)
    [[ -n "$USB4_VER" ]] || tst_brk TCONF "USB4_VER:$USB4_VER is null in ${USB4_PATH}/0-0/uevent"
  else
    tst_brk TCONF "No ${USB4_PATH}/0-0/uevent, not USB4 supported platform?"
  fi
}

check_clx() {
  local domain_port=$1
  local clx=$2
  local domain=""
  local port=""
  local clx_details=""

  tblink.sh
  if [[ "$domain_port" == *"-"* ]]; then
    tst_res TINFO "domain_port:$domain_port"
  elif [[ "$domain_port" == "auto" ]]; then
    domain_port=$(grep "tbt devices" "$CLX_OUTPUT" \
                | grep -v "contains 0 tbt" \
                | cut -d " " -f 1)
  else
    tst_brk TCONF "Invalid domain_port:$domain_port"
  fi

  domain=$(echo "$domain_port" | cut -d "-" -f 1)
  port=$(echo "$domain_port" | cut -d "-" -f 2)
  clx_details=$(grep "Domain $domain " "$CLX_OUTPUT" | grep "Port $port ")
  [[ -z "$clx_details" ]] && tst_brk TCONF "No $domain_port clx info:$clx_details"
  if [[ "$clx_details" == *"$clx"* ]]; then
    tst_res TINFO "$domain_port clx info:|$clx_details| contains $clx, pass"
  else
    tst_brk TCONF "$domain_port clx info:|$clx_details| doesn't contains $clx"
  fi
}

# Flash USB4 GR firmware and check nvm version
# Input:
#   $1: GR NVM VERSION
#   $2: GR device sysfs name
# Return 0 for true, otherwise false or die
flash_gr_nvm()
{
  local ver=$1
  local gr_device=$2
  local gr_nvm="GR_REV${ver}.bin"
  local gr_file=""
  local ori_ver=""
  local result_ver=""
  local result=""

  gr_file=$(which $gr_nvm)
  ori_ver=$(cat ${USB4_PATH}/${gr_device}/nvm_version)
  tst_res TINFO "Before flash, $gr_device nvm version:$ori_ver"
  flash_nvm "$gr_file" "$gr_device" "${USB4_PATH}/${gr_device}/${FNVM}"
  result_ver=$(cat ${USB4_PATH}/${gr_device}/nvm_version)
  result=$(cat ${USB4_PATH}/${gr_device}/nvm_version | grep $gr_device)
  if [[ -n "$result_ver" ]]; then
    tst_res TINFO "Flash $gr_device succesfully, flash:$ver,target:$result_ver"
  else
    tst_brk TCONF "Flash $gr_device mismatch, expected:$ver, actual:$result_ver"
  fi
}

. tst_test.sh
. cswitch_control.sh
