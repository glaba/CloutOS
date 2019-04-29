#!/bin/bash

gdb -ex 'add-symbol-file ../syscalls/pingpong.exe 0x8048094' bootimg
