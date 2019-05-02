#!/bin/bash

cd ../syscalls
make shell.exe
make shell
cd ..
cp -f syscalls/to_fsdir/shell fsdir/
./createfs -i fsdir -o student-distrib/filesys_img
