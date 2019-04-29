#!/bin/bash

gdb -ex 'add-symbol-file ../syscalls/shell.exe 0x8048094' bootimg
