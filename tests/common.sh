echo ------- ITEST BEGIN: $TEST_NAME ----------------------------

wait_for_qemu2()
{
    wait $qemu_pid
    echo "QEMU exited $?"
}

start_qemu()
{
    # Sometimes the unix domain socket is not properly closed
    # This loop will wait until the socket has closed otherwise qemu will
    # hang the test script

    while true;
    do
        ss -l -x | grep pb_sock > /dev/null 2>&1
        if [ $? -eq 1 ];
        then
            break;
        fi
        sleep 0.1
    done

    #( $QEMU $QEMU_FLAGS -kernel pb >> qemu.log 2>&1 ) &
    ( $QEMU $QEMU_FLAGS -kernel build-test/pb ) &
    qemu_pid=$!

}

force_recovery_mode_on()
{
    touch /tmp/pb_force_recovery
}

force_recovery_mode_off()
{
    rm -f /tmp/pb_force_recovery
}

wait_for_qemu_start()
{
    while true;
    do
        sleep 0.1
        $PB dev -l > /dev/null 2>&1
        if [ $? -eq 0 ];
        then
            break;
        fi
    done
}


wait_for_qemu()
{
    wait $qemu_pid
    if [ $? -ne 0 ];
    then
        echo "QEMU non zero result"
        exit -1
    fi
    echo "QEMU exited normally"
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

start_qemu

PB=tools/punchboot/src/build-test/punchboot
PBI=tools/pbimage/src/build/pbimage
PBCONFIG=tools/pbconfig/src/build/pbconfig

KEY1=../pki/dev_rsa_private.der
KEY2=../pki/prod_rsa_private.der

echo QEMU running, PID=$qemu_pid



