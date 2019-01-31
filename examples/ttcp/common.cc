#include "examples/ttcp/common.h"

DEFINE_string(host, "127.0.0.1", "Host");
DEFINE_int32(port, 5001, "TCP port");
DEFINE_int32(length, 65536, "Buffer length");
DEFINE_int32(number, 8192, "Number of buffers");
DEFINE_bool(transmit, false, "Transmit");
DEFINE_bool(receive, false, "Receive");
DEFINE_bool(nodelay, true, "set TCP_NODELAY");
