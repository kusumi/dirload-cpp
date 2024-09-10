#!/bin/bash

make clean
make WERROR=false WARNING_LEVEL=everything $@
