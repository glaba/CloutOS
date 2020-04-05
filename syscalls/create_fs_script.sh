make
cp to_fsdir/* ../fsdir
cd ..
./createfs -i fsdir -o student-distrib/filesys_img
cd student-distrib
make clean
make dep && sudo make
cd ../syscalls