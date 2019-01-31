#include "examples/ttcp/common.h"

#include <assert.h>

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_transmit == FLAGS_receive) {
    printf("either --transmit or --receive must be specified.\n");
    return -1;
  }

  printf("port = %d\n", FLAGS_port);
  if (FLAGS_transmit) {
    printf("buffer length = %d\n", FLAGS_length);
    printf("number of buffers = %d\n", FLAGS_number);
  } else {
    printf("accepting...\n");
  }

  if (FLAGS_transmit) {
    transmit();
  } else if (FLAGS_receive) {
    receive();
  } else {
    assert(0);
  }
}
