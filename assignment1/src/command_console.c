#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include "./../include/command_utilities.h"
#include "./helpers/logger.h"

int main(int argc, char const *argv[])
{
    // check input arguments
    if (argc != 5){
        printf("expected 4 input arguments, received %d\n", argc - 1);
        exit(EXIT_FAILURE);
    }

    // convert input arguments to local variables
    const int fdMotorX_write = atoi(argv[1]);
    const int fdMotorZ_write = atoi(argv[2]);
    const int pid_master = atoi(argv[3]);
    const int logFileId = atoi(argv[4]);
    const long refreshRate = 100000000L;  // in nanoseconds
    char msg[128];   // for log or other messages

    // convert back file descriptor into file stream
    FILE *logFile = fdopen(logFileId, "a");
    if (logFile == NULL)
    {
        perror("command console - converting log file descriptor");
        exit(errno);
    }

    snprintf(msg, sizeof(msg), "************** NEW COMMAND CONSOLE INSTANCE **************\n");
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "fd motor X write: %d\n", fdMotorX_write);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "fd motor Z write: %d\n", fdMotorZ_write);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "master pid: %d\n", pid_master);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "log file id: %d\n", logFileId);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "refresh rate: %ldns\n", refreshRate);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    
    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;

    // Initialize User Interface
    init_console_ui();

    // Infinite loop
    while (TRUE)
    {
        // Get mouse/resize commands in non-blocking mode...
        int cmd = getch();

        // If user resizes screen, re-draw UI
        if (cmd == KEY_RESIZE)
        {
            if (first_resize)
            {
                first_resize = FALSE;
            }
            else
            {
                reset_console_ui();
            }
        }
        // Else if mouse has been pressed
        else if (cmd == KEY_MOUSE)
        {
            // Check which button has been pressed...
            if (getmouse(&event) == OK)
            {
                char msg[128];
                // Vx-- button pressed
                if (check_button_pressed(vx_decr_btn, &event))
                {
                    // send command to motor
                    char cmd[] = "-";
                    snprintf(msg, sizeof(msg), "command console - decrease velocity button motor X pressed\n");
                    if (writeLogMsg(logFile, msg) == -1)
                        exit(errno);

                    int res = write(fdMotorX_write, cmd, sizeof(cmd));
                    if (res == -1) {
                        snprintf(msg, sizeof(msg), "command console - error on writing command decrease motor X\n");
                        if (writeLogMsg(logFile, msg) == -1)
                            exit(errno);
                        perror(msg);
                        exit(errno);
                    }
                }

                // Vx++ button pressed
                else if (check_button_pressed(vx_incr_btn, &event))
                {
                    // send command to motor
                    char cmd[] = "+";
                    snprintf(msg, sizeof(msg), "command console - increase velocity button motor X pressed\n");
                    if (writeLogMsg(logFile, msg) == -1)
                        exit(errno);

                    int res = write(fdMotorX_write, cmd, sizeof(cmd));
                    if (res == -1) {
                        snprintf(msg, sizeof(msg), "command console - error on writing command increase motor X\n");
                        if (writeLogMsg(logFile, msg) == -1)
                            exit(errno);
                        perror(msg);
                        exit(errno);
                    }
                }

                // Vx stop button pressed
                else if (check_button_pressed(vx_stp_button, &event))
                {
                    // send command to motor
                    char cmd[] = "0";
                    snprintf(msg, sizeof(msg), "command console - stop button motor X pressed\n");
                    if (writeLogMsg(logFile, msg) == -1)
                        exit(errno);

                    int res = write(fdMotorX_write, cmd, sizeof(cmd));
                    if (res == -1) {
                        snprintf(msg, sizeof(msg), "command console - error on writing command stop motor X\n");
                        if (writeLogMsg(logFile, msg) == -1)
                            exit(errno);
                        perror(msg);
                        exit(errno);
                    }
                }

                // Vz-- button pressed
                else if (check_button_pressed(vz_decr_btn, &event))
                {
                    // send command to motor
                    char cmd[] = "-";
                    snprintf(msg, sizeof(msg), "command console - decrease velocity button motor Z pressed\n");
                    if (writeLogMsg(logFile, msg) == -1)
                        exit(errno);

                    int res = write(fdMotorZ_write, cmd, sizeof(cmd));
                    if (res == -1) {
                        snprintf(msg, sizeof(msg), "command console - error on writing command decrease motor Z\n");
                        if (writeLogMsg(logFile, msg) == -1)
                            exit(errno);
                        perror(msg);
                        exit(errno);
                    }
                }

                // Vz++ button pressed
                else if (check_button_pressed(vz_incr_btn, &event))
                {
                    // send command to motor
                    char cmd[] = "+";
                    snprintf(msg, sizeof(msg), "command console - increase velocity button motor Z pressed\n");
                    if (writeLogMsg(logFile, msg) == -1)
                        exit(errno);

                    int res = write(fdMotorZ_write, cmd, sizeof(cmd));
                    if (res == -1) {
                        snprintf(msg, sizeof(msg), "command console - error on writing command increase motor Z\n");
                        if (writeLogMsg(logFile, msg) == -1)
                            exit(errno);
                        perror(msg);
                        exit(errno);
                    }
                }

                // Vz stop button pressed
                else if (check_button_pressed(vz_stp_button, &event))
                {
                    // send command to motor
                    char cmd[] = "0";
                    snprintf(msg, sizeof(msg), "command console - stop button motor Z pressed\n");
                    if (writeLogMsg(logFile, msg) == -1)
                        exit(errno);

                    int res = write(fdMotorZ_write, cmd, sizeof(cmd));
                    if (res == -1) {
                        snprintf(msg, sizeof(msg), "command console - error on writing command stop motor Z\n");
                        if (writeLogMsg(logFile, msg) == -1)
                            exit(errno);
                        perror(msg);
                        exit(errno);
                    }

                }

                // close button pressed
                else if (check_button_pressed(close_btn, &event))
                {
                    printf("close everything");
                    fflush(stdout);
                    
                    snprintf(msg, sizeof(msg), "command console - CLOSE button pressed\n");
                    if (writeLogMsg(logFile, msg) == -1)
                        exit(errno);

                    if (kill(pid_master, SIGTERM) == -1) {
                        snprintf(msg, sizeof(msg), "command console - error on writing command to close\n");
                        if (writeLogMsg(logFile, msg) == -1)
                            exit(errno);
                        perror(msg);
                        exit(errno);
                    }
                }
            }
        }
        
        refresh();

        struct timespec req = {
            .tv_sec = 0,
            .tv_nsec = refreshRate
        };
        char msg[64];
        if (nanosleep(&req, NULL))
        {
            // if not because of an incoming signal
            if (errno != EINTR) {
                snprintf(msg, sizeof(msg), "command console - error on nanosleep\n");
                if (writeLogMsg(logFile, msg) == -1)
                    exit(errno);
                perror(msg);
                exit(errno);
            }
        }
    }

    // Terminate
    endwin();
    return 0;
}
