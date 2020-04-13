#include <pb/pb.h>
#include <bpak/keystore.h>

extern struct bpak_keystore keystore_pb;

struct bpak_keystore *pb_keystore(void)
{
    return &keystore_pb;
}
