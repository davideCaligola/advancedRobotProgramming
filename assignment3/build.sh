#!/bin/bash
mkdir -p ./bin
gcc ./src/master.c -lncurses -lpthread -lrt -o ./bin/master
gcc ./src/processA_server.c -lbmp -lm -lrt -lncurses -lpthread -o ./bin/processA_server
gcc ./src/processA_client.c -lbmp -lm -lrt -lncurses -lpthread -o ./bin/processA_client
gcc ./src/processB.c -lbmp -lncurses -lrt -lm -lpthread -o ./bin/processB
