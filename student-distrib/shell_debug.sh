#!/bin/bash

gdb -ex 'add-symbol-file ../syscalls/sigtest.exe 0x8048094' bootimg
