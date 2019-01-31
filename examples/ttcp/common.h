#pragma once

#include <gflags/gflags.h>
#include <stdint.h>
#include <string>

DECLARE_string(host);
DECLARE_int32(port);
DECLARE_int32(length);
DECLARE_int32(number);
DECLARE_bool(transmit);
DECLARE_bool(receive);
DECLARE_bool(nodelay);

bool parseCommandLine(int argc, char* argv[]);

struct SessionMessage {
  int32_t number;
  int32_t length;
} __attribute__((__packed__));

struct PayloadMessage {
  int32_t length;
  char data[0];
};

void transmit();

void receive();
