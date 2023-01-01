#! /usr/bin/bash
mkdir -p ./bin
gcc ./src/inspection_console.c ./src/helpers/logger.c -lncurses -lm -o ./bin/inspection
gcc ./src/command_console.c ./src/helpers/logger.c -lncurses -o ./bin/command
gcc ./src/motor.c ./src/helpers/logger.c -o ./bin/motor
gcc ./src/world.c ./src/helpers/logger.c -o ./bin/world
gcc ./src/master.c ./src/helpers/logger.c -o ./bin/master