echo ------- ITEST BEGIN: $TEST_NAME ----------------------------

( $QEMU $QEMU_FLAGS -kernel pb >> qemu.log 2>&1 ) &
qemu_pid=$!
PB=tools/punchboot/punchboot
PBI=tools/pbimage/pbimage

KEY1=../pki/dev_rsa_private.der

#echo QEMU running, PID=$qemu_pid

wait_for_qemu_start()
{
    while true;
    do
        $PB boot -l > /dev/null 2>&1
        if [ $? -eq 0 ];
        then
            break;
        fi
        sleep 0.1
    done
}

wait_for_qemu()
{
    wait $qemu_pid
    if [ $? -ne 0 ];
    then
        exit -1
    fi

}

test_end_error()
{
    echo ------- ITEST END ERROR: $TEST_NAME ------------------------
    $PB boot -r
    wait_for_qemu
    exit -1
}

test_end_ok()
{

    echo ------- ITEST END OK: $TEST_NAME ---------------------------
    $PB boot -r
    wait_for_qemu
}
