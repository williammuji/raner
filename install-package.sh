#!/bin/sh
set -ex
wget https://github.com/google/glog/archive/v0.3.5.tar.gz -O /tmp/glog-0.3.5.tar.gz
cd /tmp
tar -xzvf glog-0.3.5.tar.gz
cd glog-0.3.5 && ./configure && make && sudo make install
wget https://github.com/google/googletest/archive/release-1.8.1.tar.gz -O /tmp/googletest-release-1.8.1.tar.gz
cd /tmp
tar -xzvf googletest-release-1.8.1.tar.gz
cd googletest-release-1.8.1 && cmake . && make && sudo make install
