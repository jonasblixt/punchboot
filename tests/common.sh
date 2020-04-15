echo ------- ITEST BEGIN: $TEST_NAME ----------------------------

source ../.config

wait_for_qemu2()
{
    wait $qemu_pid
    echo "QEMU exited $?"
}

reset_disk()
{
	dd if=/dev/zero of=$CONFIG_QEMU_VIRTIO_DISK bs=1M \
		count=$CONFIG_QEMU_VIRTIO_DISK_SIZE_MB > /dev/null 2>&1
	sync
}

start_qemu()
{
    # Sometimes the unix domain socket is not properly closed
    # This loop will wait until the socket has closed otherwise qemu will
    # hang the test script

    while true;
    do
        ss -l -x | grep pb.sock > /dev/null 2>&1
        if [ $? -eq 1 ];
        then
            break;
        fi
        sleep 0.1
    done

    #( $QEMU $QEMU_FLAGS -kernel pb >> qemu.log 2>&1 ) &
    ( $QEMU $QEMU_FLAGS -kernel build-qemutest/pb ) &
    qemu_pid=$!

}

force_recovery_mode_on()
{
    touch /tmp/pb_force_command_mode
}

force_recovery_mode_off()
{
    rm -f /tmp/pb_force_command_mode
}

auth()
{
    $PB auth --token 2456a34c-d17a-3b6d-9157-bbe585b48e7b.token --key-id 0xa90f9680 --transport socket
}

wait_for_qemu_start()
{
    while true;
    do
        sleep 0.1
        $PB dev --transport socket --show > /dev/null 2>&1
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
    $PB dev --transport socket --reset
    wait_for_qemu
    exit -1
}

test_end_ok()
{

    echo ------- ITEST END OK: $TEST_NAME ---------------------------
    $PB dev --transport socket --reset
    wait_for_qemu
}

start_qemu

PB=punchboot
PBSTATE=pbstate
BOOT_A=2af755d8-8de5-45d5-a862-014cfa735ce0
BOOT_B=c046ccd8-0f2e-4036-984d-76c14dc73992
echo QEMU running, PID=$qemu_pid



