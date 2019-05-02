#!/bin/bash

cd ../syscalls
make window.exe
make window
cd ..
cp -f syscalls/to_fsdir/window fsdir/
./createfs -i fsdir -o student-distrib/filesys_img
