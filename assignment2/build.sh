#!/bin/bash
mkdir -p ./bin
gcc ./src/master.c -lncurses -lpthread -o ./bin/master
gcc ./src/processA.c -lbmp -lm -lncurses -lpthread -o ./bin/processA
gcc ./src/processB.c -lbmp -lncurses -lm -lpthread -o ./bin/processB
