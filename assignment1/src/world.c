/* task to add noise to the motor measurements
    Inputs:
        - int fd_motorX: file device for reading current motor X position
        - int fd_motorZ: file device for reading current motor Z position
        - int fd_inspect: file device for writing current X,Z positions with noise
        - char fd_log: path to the file for writing logs
*/

#include <stdio.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include "./helpers/logger.h"

float getNoisySignal(float signal, float noiseAmplitude);

int main(int argc, const char *argv[])
{
    // check input arguments
    if (argc != 7){
        printf("world - expected 6 input arguments, received %d\n", argc - 1);
        exit(EXIT_FAILURE);
    }

    // convert input arguments to local variables
    const int fdMotorX_read = atoi(argv[1]);
    const int fdMotorZ_read = atoi(argv[2]);
    const int fdInspect_write = atoi(argv[3]);
    const int logFileId = atoi(argv[4]);
    const int samplingTime = atoi(argv[5]);
    float noiseAmplitude;
    sscanf(argv[6],"%f",&noiseAmplitude);
    const long logPeriod = 200000000L; // minimum period between two log entries
    char msg[64];   // for log or other messages

    // convert back file descriptor into file stream
    FILE *logFile = fdopen(logFileId, "a");
    if (logFile == NULL)
    {
        perror("world - converting log file descriptor");
        exit(errno);
    }

    snprintf(msg, sizeof(msg), "************** NEW WORLD INSTANCE **************\n");
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "fd motor x read: %d\n", fdMotorX_read);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "fd motor z read: %d\n", fdMotorZ_read);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "fd inspection console write: %d\n", fdInspect_write);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "log file id: %d\n", logFileId);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "samplingTime: %d\n", samplingTime);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "noiseAmplitude: %f\n", noiseAmplitude);
    if(writeLogMsg(logFile, msg) == -1)
        exit(errno);

    struct pollfd poll_motorReads[2] = {
        {
            .fd = fdMotorX_read,
            .events = POLLIN
        },
        {
            .fd = fdMotorZ_read,
            .events = POLLIN
        }
    };

    unsigned short samplingPeriod = 1000; // sampling time in milliseconds
    const unsigned int POS_SIZE = 6;  // size of positions to send
    float positionX = 0.0;
    float positionX_noise = 0.0;
    float positionZ = 0.0;
    float positionZ_noise = 0.0;

    // time structures for timing the log entry
    struct timespec timeStart;
    struct timespec timeEnd;
    long int logTimePeriod = (samplingTime > logPeriod)
        ? (long)samplingTime
        : logPeriod;    // minimum period of time in nanoseconds between
                        // two consecutive entries in the log file

    // get start time
    if(clock_gettime(CLOCK_BOOTTIME, &timeStart))
    {
        // char msg[64];
        sprintf(msg, "world - getting logger start time\n");
        if(writeLogMsg(logFile, msg) == -1)
            exit(errno);
        perror(msg);
        exit(errno);
    }

    while(1) 
    {
        // read data from the two motors
        int res = poll(poll_motorReads, 2, samplingPeriod);
        
        // error handling
        if (res == -1) {
            snprintf(msg, sizeof(msg), "world - poll for motors\n");
            if(writeLogMsg(logFile, msg) == -1)
                exit(errno);
            perror(msg);
            exit(errno);
        }
        
        // data to read.
        // It does not matter the order, since if ready, both of them will be sent together
        if (res > 0) {
            // data from motor x
            if (poll_motorReads[0].revents & POLLIN) {
                char positionX_read[POS_SIZE];
                int len = read(poll_motorReads[0].fd, &positionX_read, POS_SIZE);

                // error handling
                if (len == -1) {
                    snprintf(msg, sizeof(msg), "world - reading motorX position\n");
                    if(writeLogMsg(logFile, msg) == -1)
                        exit(errno);
                    perror(msg);
                    exit(errno);
                }

                else if (len > 0) { // convert position from string to float
                    positionX = strtof(positionX_read, NULL);
                }
            }

            // data from motor z
            if (poll_motorReads[1].revents & POLLIN) {
                char positionZ_read[POS_SIZE];
                int len = read(poll_motorReads[1].fd, &positionZ_read, POS_SIZE);
                // char msg[64];
                
                // error handling
                if (len == -1) {
                    snprintf(msg, sizeof(msg), "world - reading motorZ position\n");
                    if(writeLogMsg(logFile, msg) == -1)
                        exit(errno);
                    perror(msg);
                    exit(errno);
                }

                else if (len > 0) { // convert position from string to float
                    positionZ = strtof(positionZ_read, NULL);
                }
            }

            // add noise to motor positions only when a new position comes
            positionX_noise = getNoisySignal(positionX, noiseAmplitude);
            positionZ_noise = getNoisySignal(positionZ, noiseAmplitude);

            // printf("world - positionX_noise: %f\n", positionX_noise);
            // fflush(stdout);
            // printf("world - positionZ_noise: %f\n", positionZ_noise);
            // fflush(stdout);

            // send noisy position to inspection console only when a new position comes

            // communication protocol with console:
            //          positionX,positionZ
            // maximum value for each position 99.99
            const int POS_SEND_SIZE = 12;
            char positions_send[POS_SEND_SIZE];
            
            snprintf(positions_send, POS_SEND_SIZE, "%.2f,%.2f",positionX_noise, positionZ_noise);
            int retWrite = write(fdInspect_write, positions_send, POS_SEND_SIZE);

            // error handling
            if (retWrite != POS_SEND_SIZE) {
                snprintf(msg, sizeof(msg), "world - not able to write all position string\n");
                if(writeLogMsg(logFile, msg) == -1)
                    exit(errno);
                perror(msg);
                exit(EXIT_FAILURE);
            }

            if (retWrite == -1) {
                snprintf(msg, sizeof(msg), "world - writing positions for sending to inspection console\n");
                if(writeLogMsg(logFile, msg) == -1)
                    exit(errno);
                perror(msg);
                exit(errno);
            }

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
                // write a log entry
                char logMsg[128];
                snprintf(logMsg, sizeof(logMsg),
                    "world - current position (x,z): (%f,%f)\n", positionX, positionZ);
                if(writeLogMsg(logFile, logMsg) == -1)
                    exit(errno);
                snprintf(logMsg, sizeof(logMsg),
                    "world - current position with noise (x,z): (%f,%f)\n", positionX_noise, positionZ_noise);
                if(writeLogMsg(logFile, logMsg) == -1)
                    exit(errno);

                // reset start time for logging
                if(clock_gettime(CLOCK_BOOTTIME, &timeStart))
                {
                    sprintf(msg, "world - getting logger end time\n");
                    if(writeLogMsg(logFile, msg) == -1)
                        exit(errno);
                    perror(msg);
                    exit(errno);
                }
            }
        }

    }


    return 0;
}

float getNoisySignal(float signal, float noiseAmplitude)
{
    return signal + noiseAmplitude * ((float)rand()/(float)RAND_MAX) - noiseAmplitude / 2.0;
}
