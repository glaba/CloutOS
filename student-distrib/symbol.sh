#!/bin/bash

gdb -ex 'add-symbol-file ../syscalls/window.exe 0x8048094' -ex c bootimg
