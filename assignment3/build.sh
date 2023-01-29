#!/bin/bash
mkdir -p ./bin
gcc ./src/master.c -lncurses -lpthread -o ./bin/master
gcc ./src/processA_server.c -lbmp -lm -lncurses -lpthread -o ./bin/processA_server
gcc ./src/processA_client.c -lbmp -lm -lncurses -lpthread -o ./bin/processA_client
gcc ./src/processB.c -lbmp -lncurses -lm -lpthread -o ./bin/processB
