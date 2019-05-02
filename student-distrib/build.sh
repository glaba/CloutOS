#!/bin/bash

cd ../syscalls
make chat.exe
make chat
cd ..
cp -f syscalls/to_fsdir/chat fsdir/
./createfs -i fsdir -o student-distrib/filesys_img
