/* task to simulate a motor
    Inputs:
        - float baseVelocity: base velocity
        - int fd_cmd: file device for reading command
        - int fd_world: file device for writing current position
        - char fd_log: file device for writing logs
*/

#define _POSIX_C_SOURCE 199309
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include "./helpers/logger.h"

/*--------------------------------------------------------------------
                            GLOVAL VARIABLES
--------------------------------------------------------------------*/
unsigned char reset = 0;
char velocityGain = 0;
FILE *logFile;
// additional string message in case of reset command
char resettingMsg[12];

/*--------------------------------------------------------------------
                            SIGNAL HANDLERS
--------------------------------------------------------------------*/
/* Handler for the stop signal (SIGUSR1)*/
void stopHandler(int signo) {
    // printf("-------- signal SIGUSR1 -----------\n");
    // fflush(stdout);
    signal(SIGUSR1, stopHandler);

    velocityGain = 0;
    reset = 0;
    resettingMsg[0] = '\0';
    if (writeLogMsg(logFile, "signal SIGUSR1 received - STOPPING\n") == -1) {
        perror("Motor stop handler");
        exit(errno);
    }
}

/* Handler for the reset signal (SIGUSR2)*/
void resetHandler(int signo) {
    // printf("-------- signal SIGUSR2 -----------\n");
    // fflush(stdout);
    signal(SIGUSR2, resetHandler);
    
    reset = 1;
    if (writeLogMsg(logFile, "signal SIGUSR2 received - RESETTING\n") == -1) {
        perror("Motor reset handler");
        exit(errno);
    }
}

/*--------------------------------------------------------------------
                    HELPER FUNCTION DECLARATIONS
--------------------------------------------------------------------*/
/*given the current velocity gain and the command, it returns the new velocity gain*/
unsigned char getVelocityGain(char, char*);


/*--------------------------------------------------------------------
                                MAIN
--------------------------------------------------------------------*/
int main(int argc, const char *argv[])
{
    // check input arguments
    if (argc != 9){
        printf("expected 8 input arguments, received %d", argc - 1);
        exit(EXIT_FAILURE);
    }

    // convert input arguments to local variables
    const float baseVelocity = (float)atof(argv[1]);
    const float pos_min = (float)atof(argv[2]); // minimum motor position limit
    const float pos_max = (float)atof(argv[3]); // maximum motor position limit
    const int fdCmd_read = atoi(argv[4]);
    const int fdWorld_write = atoi(argv[5]);
    const int logFileId = atoi(argv[6]);
    const char *motorName = argv[7];
    const int samplingTime = atoi(argv[8]); // sampling time in milliseconds
    char msg[128];  // for log or other messages

    // convert back file descriptor into file stream
    logFile = fdopen(logFileId, "a");
    if (logFile == NULL)
    {
        char msg[64];
        sprintf(msg, "%s - converting log file descriptor", motorName);
        perror(msg);
        exit(errno);
    }

    snprintf(msg, sizeof(msg), "************** NEW MOTOR %s INSTANCE **************\n", motorName);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "base velocity: %f\n", baseVelocity);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "pos_min: %f\n", pos_min);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "pos_max: %f\n", pos_max);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "fd command read: %d\n", fdCmd_read);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "fd world write: %d\n", fdWorld_write);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "log file id: %d\n", logFileId);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "motor name: %s\n", motorName);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "sampling time: %d\n", samplingTime);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);

    // clear reset message
    memset(resettingMsg, '\0', sizeof(resettingMsg));

    // motor parameter
    float position_new = 0;
    float position_old = position_new;

    // listener to SIGUSR1 for stop
    if (signal(SIGUSR1, stopHandler) == SIG_ERR)
    {
        sprintf(msg, "%s - SIGUSR1 not handled", motorName);
        if(writeLogMsg(logFile, msg) == -1)
            exit(errno);
        perror(msg);
        exit(errno);
    }

    // listener to SIGUSR2 for reset
    if (signal(SIGUSR2, resetHandler) == SIG_ERR)
    {
        sprintf(msg, "%s - SIGUSR2 not handled", motorName);
        if (writeLogMsg(logFile, msg) == -1)
            exit(errno);
        perror(msg);
        exit(errno);
    }

    // poll structure for reading commands in normal mode
    struct pollfd poll_cmdRead = {
        .fd = fdCmd_read,
        .events = POLLIN
    };

    // time structure for nanosleep in reset mode
    struct timespec reqResetNanoSleep = {
        .tv_sec = samplingTime / 1000,
        .tv_nsec = (samplingTime % 1000) * 1000000
    };

    // time structures for timing the log entry
    struct timespec timeStart;
    struct timespec timeEnd;
    long int logTimePeriod = (samplingTime > 200000000L)
        ? (long)samplingTime
        : 200000000L; // minimum period of time in nanoseconds between
                                         // two consecutive entries in the log file

    // get start time
    if(clock_gettime(CLOCK_BOOTTIME, &timeStart))
    {
        sprintf(msg, "%s - getting logger start time", motorName);
        if (writeLogMsg(logFile, msg) == -1)
            exit(errno);
        perror(msg);
        exit(errno);
    }

    while(1)
    {
        /*--------------------------------------------------------------------
                                NORMAL WORKING MODE
        --------------------------------------------------------------------*/
        if(!reset)
        {
            int res = poll(&poll_cmdRead, 1, samplingTime);
            
            // error handling
            if (res == -1) {
                // if not because of an incoming signal
                if (errno != EINTR) {
                    sprintf(msg, "%s - polling command console", motorName);
                    if(writeLogMsg(logFile, msg) == -1)
                        exit(errno);
                    perror(msg);
                    exit(errno);
                }
            }
            
            // New command came in - update velocity gain
            else if (poll_cmdRead.revents & POLLIN) {
                char cmd[2];
                int len = read(fdCmd_read, &cmd, sizeof(cmd));
                if (len == -1) {
                    sprintf(msg, "%s - reading command console", motorName);
                    if (writeLogMsg(logFile, msg) == -1)
                        exit(errno);
                    perror(msg);
                    exit(errno);
                }
                else if (len > 0) {
                    velocityGain = getVelocityGain(velocityGain, cmd);

                    sprintf(msg, "%s - new velocity gain: %d\n", motorName, velocityGain);
                    if(writeLogMsg(logFile, msg) == -1)
                        exit(errno);
                    // printf("velocityGain: %d\n", velocityGain);
                    // fflush(stdout);
                }
            }

            // update position_new
            position_new += (float)velocityGain * baseVelocity * (float)samplingTime / 1000.0;
            
            // saturate position_new within limits
            if (position_new > pos_max) {
                position_new = pos_max;
                velocityGain = 0;
            }
            if (position_new < pos_min) {
                position_new = pos_min;
                velocityGain = 0;
            }

            // printf("Current position_new: %f\n", position_new);
        }

        /*--------------------------------------------------------------------
                                    RESET MODE
        --------------------------------------------------------------------*/
        else
        {
            // printf("---------- starting reset procedure -------\n");
            // fflush(stdout);
            velocityGain = -1;
            
            char msg[64];
            strcpy(resettingMsg,"resetting ");
            
            position_new += (float)velocityGain * baseVelocity * (float)samplingTime / 1000.0;
            // printf("position_new : %f pos_min: %f\n", position_new, pos_min);
            // fflush(stdout);
            if (position_new < pos_min)
            {
                // printf("---------- reset procedure terminated -------\n");
                // fflush(stdout);
                position_new = pos_min;
                
                velocityGain = 0;
                snprintf(msg, sizeof(msg), "%s - new velocity gain: %d\n", motorName, velocityGain);
                if (writeLogMsg(logFile, msg) == -1)
                    exit(errno);
                
                reset = 0;
                snprintf(msg, sizeof(msg), "%s - reset procedure terminated\n", motorName);
                if (writeLogMsg(logFile, msg) == -1)
                    exit(errno);
                resettingMsg[0] = '\0';
            }
            
            int retResetNanoSleep = nanosleep(&reqResetNanoSleep, NULL);
            if (retResetNanoSleep)
            {
                if (errno != EINTR) {
                    snprintf(msg, sizeof(msg), "%s - reset nonosleep", motorName);
                    if (writeLogMsg(logFile, msg) == -1)
                        exit(errno);
                    perror(msg);
                    exit(errno);
                }
            }
        }

        /*-------------------------------------------------
                        send data to world
        --------------------------------------------------*/
        // send data only if there is a new position
        if (position_new != position_old) {
            // update old position
            position_old = position_new;

            // maximum value 99.99
            const int POS_SEND_SIZE = 6;
            char position_send[POS_SEND_SIZE];
            snprintf(position_send, POS_SEND_SIZE, "%.2f",position_new);
            int retWrite = write(fdWorld_write, position_send, POS_SEND_SIZE);
            
            if (retWrite != POS_SEND_SIZE) {
                sprintf(msg, "%s not able to write position_new", motorName);
                if (writeLogMsg(logFile, msg) == -1)
                    exit(errno);
                perror(msg);
                exit(EXIT_FAILURE);
            }

            if (retWrite == -1) {
                sprintf(msg, "%s writing position_new", motorName);
                if(writeLogMsg(logFile, msg) == -1)
                    exit(errno);
                perror(msg);
                exit(errno);
            }

            // get elapsed time for logging
            if(clock_gettime(CLOCK_BOOTTIME, &timeEnd))
            {
                sprintf(msg, "%s - getting logger end time", motorName);
                if (writeLogMsg(logFile, msg) == -1)
                    exit(errno);
                perror(msg);
                exit(errno);
            }

            // elapsed time from the latest log (in milliseconds)
            long elapsedTime = (long)(timeEnd.tv_sec - timeStart.tv_sec) * 1000000000L +
                                    (timeEnd.tv_nsec - timeStart.tv_nsec);
            if (elapsedTime > logTimePeriod) {
                // write a log entry
                snprintf(msg, sizeof(msg),
                    "%s - %scurrent position_new %f\n", motorName, resettingMsg, position_new);
                if (writeLogMsg(logFile, msg) == -1)
                    exit(errno);

                // reset start time for logging
                if(clock_gettime(CLOCK_BOOTTIME, &timeStart))
                {
                    sprintf(msg, "%s - getting logger start time", motorName);
                    if (writeLogMsg(logFile, msg) == -1)
                        exit(errno);
                    perror(msg);
                    exit(errno);
                }
            }
        }
    }
    
    return 0;
}

unsigned char getVelocityGain(char initialVelocityGain, char *cmd)
{
    if (strcmp(cmd,"+") == 0){
        return (++initialVelocityGain < 3)
            ? initialVelocityGain
            : 2;
    }
    
    else if (strcmp(cmd,"-") == 0) {
        return (--initialVelocityGain > -3)
            ? initialVelocityGain
            : -2;
    }
    else
        return 0;
}
