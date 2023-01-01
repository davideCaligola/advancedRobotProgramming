#include <time.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
#include "./../include/inspection_utilities.h"
#include "./helpers/logger.h"

int main(int argc, char const *argv[])
{
    // check input arguments
    if (argc != 6){
        printf("inspection console - expected 5 input arguments, received %d\n", argc - 1);
        exit(EXIT_FAILURE);
    }

    // convert input arguments to local variables
    const int fdWorld_read = atoi(argv[1]);
    const int pid_motorX = atoi(argv[2]);
    const int pid_motorZ = atoi(argv[3]);
    const int logFileId = atoi(argv[4]);
    const int samplingTime = atoi(argv[5]); // sampling time in milliseconds
    const long logPeriod = 200000000L;  // minimum period between two consecutive log entries
    char msg[128];  // for log or other messages

    // convert back file descriptor into file stream
    FILE *logFile = fdopen(logFileId, "a");
    if (logFile == NULL)
    {
        perror("inspection console - converting log file descriptor");
        exit(errno);
    }

    snprintf(msg, sizeof(msg), "************** NEW INSPECTION CONSOLE INSTANCE **************\n");
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "fd world read: %d\n", fdWorld_read);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "pid motor X: %d\n", pid_motorX);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "pid motor Z: %d\n", pid_motorZ);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "log file id: %d\n", logFileId);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "sampling time %d\n", samplingTime);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "log period %ldns\n", logPeriod);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);

    // structure for reading data from world
    struct pollfd poll_worldRead = {
        .fd = fdWorld_read,
        .events = POLLIN
    };
    const unsigned int POS_SIZE = 12; // buffer size for reading
                                      // motor positions from world
    // motor positions
    float positionX = 0.0;
    float positionZ = 0.0;
    // motor positions for UI
    int positionX_ui = 0;
    int positionZ_ui = 0;

    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;

    // End-effector coordinates
    float ee_x, ee_y;

    // Initialize User Interface
    init_console_ui();

       // time structures for timing the log entry
    struct timespec timeStart;
    struct timespec timeEnd;
    long int logTimePeriod = (samplingTime > logPeriod)
        ? (long)samplingTime
        : logPeriod;   // minimum period of time in nanoseconds between
                        // two consecutive entries in the log file

    // get start time
    if(clock_gettime(CLOCK_BOOTTIME, &timeStart))
    {
        sprintf(msg, "world - getting logger start time\n");
        if(writeLogMsg(logFile, msg) == -1)
            exit(errno);
        perror(msg);
        exit(errno);
    }

    // Infinite loop
    while (TRUE)
    {
        // read data from world
        int res = poll(&poll_worldRead, 1, samplingTime);
        
        // error handling
        if (res == -1) {
            // if not because of an incoming signal
            if (errno != EINTR) {
                snprintf(msg, sizeof(msg), "inspection console - poll reading world\n");
                if(writeLogMsg(logFile, msg) == -1)
                    exit(errno);
                perror(msg);
                exit(errno);
            }
        }
        
        // data to read
        if (res > 0) {
            // data from motor x
            if (poll_worldRead.revents & POLLIN) {
                char positions_read[POS_SIZE];
                int len = read(poll_worldRead.fd, &positions_read, POS_SIZE);

                // error handling
                if (len == -1) {
                    snprintf(msg, sizeof(msg), "world - reading motorX position\n");
                    if (writeLogMsg(logFile, msg) == -1)
                        exit(errno);
                    perror(msg);
                    exit(errno);
                }

                else if (len > 0) { // convert position from string to float
                    sscanf(positions_read, "%f,%f", &positionX, &positionZ);
                    // positions for UI
                    positionX_ui = (int)roundf(positionX);
                    positionZ_ui = (int)roundf(positionZ);
                }

                /*------------------------------------------------------
                        write on log only when a new value comes
                -------------------------------------------------------*/
                // get elapsed time for logging
                if(clock_gettime(CLOCK_BOOTTIME, &timeEnd))
                {
                    sprintf(msg, "world - getting start time\n");
                    if(writeLogMsg(logFile, msg) == -1)
                        exit(errno);
                    perror(msg);
                    exit(errno);
                }

                // elapsed time from the latest log (in milliseconds)
                long elapsedTime = (long)(timeEnd.tv_sec - timeStart.tv_sec) * 1000000000L +
                                        (timeEnd.tv_nsec - timeStart.tv_nsec);
                if (elapsedTime > logTimePeriod) {
                    snprintf(msg, sizeof(msg),
                        "inspection console - received positions (x,z): (%f,%f)\n", positionX, positionZ);
                    if(writeLogMsg(logFile, msg) == -1)
                        exit(errno);

                    // reset start time for logging
                    if(clock_gettime(CLOCK_BOOTTIME, &timeStart))
                    {
                        sprintf(msg, "inspection console - getting logger end time\n");
                        if(writeLogMsg(logFile, msg) == -1)
                            exit(errno);
                        perror(msg);
                        exit(errno);
                    }
                }
            }
        }

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

                // STOP button pressed
                if (check_button_pressed(stp_button, &event))
                {
                    // update log file
                    if (writeLogMsg(logFile, "inspection console - STOP button pressed\n") == -1)
                        exit(errno);
                    
                    // send singal to motors for stopping them
                    kill(pid_motorX, SIGUSR1);
                    kill(pid_motorZ, SIGUSR1);
                }

                // RESET button pressed
                else if (check_button_pressed(rst_button, &event))
                {
                    // update log file
                    if (writeLogMsg(logFile, "inspection console - RESET button pressed\n") == -1)
                        exit(errno);
                    
                    // send singal to motors for position reset
                    kill(pid_motorX, SIGUSR2);
                    kill(pid_motorZ, SIGUSR2);
                }
            }
        }

        // To be commented in final version...
        // switch (cmd)
        // {
        // case KEY_LEFT:
        //     ee_x--;
        //     break;
        // case KEY_RIGHT:
        //     ee_x++;
        //     break;
        // case KEY_UP:
        //     ee_y--;
        //     break;
        // case KEY_DOWN:
        //     ee_y++;
        //     break;
        // default:
        //     break;
        // }

        // Update UI
        update_console_ui(&positionX, &positionZ);
    }

    // Terminate
    endwin();
    return 0;
}

