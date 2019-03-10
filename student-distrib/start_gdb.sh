#!/bin/bash

gdb -ex 'target remote 10.0.2.2:1234' -ex c bootimg
