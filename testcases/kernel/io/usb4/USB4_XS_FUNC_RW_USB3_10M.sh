TST_TESTFUNC=do_test

set_configuration()
{
  # Default size 1MB
  : ${BLOCK_SIZE:="1MB"}
  : ${BLOCK_COUNT:="1"}
  : ${TIME:="2"}
  : ${DEVICE_TP:="device"}
  : ${MODE:="NA"}

  local OPTIND
  while getopts :p:d:b:c:t:m:h arg
    do
  	  case $arg in
        p)
          PROTOCOL_TYPE=$OPTARG
          ;;
        d)
          DEVICE_TP=$OPTARG
          ;;
        b)
          BLOCK_SIZE=$OPTARG
          ;;
        c)
          BLOCK_COUNT=$OPTARG
          ;;
        t)
          TIME=$OPTARG
          ;;
        m)
          MODE=$OPTARG
          ;;
        h)
          usage && exit 0
          ;;
        \?)
          usage
      	  die "Invalid Option -$OPTARG"
      	  ;;
    	:)
      	  usage
      	  die "Option -$OPTARG requires an argument."
      	  ;;
  		esac
	done
}

perform_io()
{
  local test_file=""
  local iozone_bin=""
    
  # Prepare stage
  check_auto_connect
  check_test_env
  check_free_partition "$BLOCK_SIZE" "$BLOCK_COUNT"

  is_only_usb4_device_connected \
        || die "Not only USB4 device connected"


  # Enable authorized, find device connected by thunderbolt
  enable_authorized
  sleep 5
  find_tbt_device "$BLOCK_SIZE" "$BLOCK_COUNT" "$PROTOCOL_TYPE" "$DEVICE_TP"
  [[ -n "$DEVICE_NODE" ]] || die "No $PROTOCOL_TYPE $DEVICE_TP node:$DEVICE_NODE"


  # Generate test folder and test file
  [[ -e "$TEMP_DIR" ]] || block_test "fail to create temporary directory!"
  tst_res TINFO "TEMP_DIR: $TEMP_DIR"
  test_file=$(generate_test_file "$BLOCK_SIZE" "$BLOCK_COUNT" "$TEMP_DIR")

  mount_dev "$DEVICE_NODE" "$MOUNT_FOLDER"

  # Read write test in request times
  for ((i=1; i <= TIME; i++)); do
    tst_res TINFO "------------------------$i times read write test:"
    write_test_with_file "$RW_FILE" "$test_file" \
    "$BLOCK_SIZE" "$BLOCK_COUNT"
    read_test_with_file "$RW_FILE" "$test_file" \
    "$BLOCK_SIZE" "$BLOCK_COUNT"
  done
  rm -rf "$TEMP_DIR"
  

}

do_test()
{
   tst_res TINFO "Running TBT test RW to pendrive"
   set_configuration -b 1MB -c 10 -p 3.0 -d flash -t 2
	 perform_io
}

. usb4_common_lib.sh
tst_run



