#!/bin/bash
source tests/common.sh
wait_for_qemu_start

dd if=/dev/urandom of=/tmp/random_data bs=64k count=1

$PB -t socket part write /tmp/random_data 2af755d8-8de5-45d5-a862-014cfa735ce0
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

dd if=/dev/urandom of=/tmp/other_data bs=64k count=1

# Leave the handles at EOF, part_verify should still verify the whole file.
# Without a rewind 0 bytes are verified, which always passes.
python3 - <<'EOF'
from punchboot import Session, PartVerifyError
s = Session(socket_path="/tmp/pb.sock")
with open("/tmp/random_data", "rb") as f:
    f.read()
    s.part_verify(f, "2af755d8-8de5-45d5-a862-014cfa735ce0")
with open("/tmp/other_data", "rb") as f:
    f.read()
    try:
        s.part_verify(f, "2af755d8-8de5-45d5-a862-014cfa735ce0")
    except PartVerifyError:
        pass
    else:
        raise SystemExit("verify of mismatching data passed")
EOF
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi
test_end_ok
