#include "./logger.h"

int writeLogMsg(FILE *logFile, char* description)
{
    char logMsg[100];
    struct timespec ts;
    struct tm *dateTime;

    int ret = clock_gettime(CLOCK_REALTIME, &ts);
    if (ret == -1) {
        perror("logger - getting time");
        return -1;
    }
    
    long milliseconds = ts.tv_nsec / 1000000;
    dateTime = localtime(&ts.tv_sec);
    if (dateTime == NULL) {
        perror("logger - localtime");
        return -1;
    }
    sprintf(logMsg, "%d-%02d-%02d %02d:%02d:%02d.%03ld %s",
        dateTime->tm_year + 1900, dateTime->tm_mon, dateTime->tm_mday,
        dateTime->tm_hour, dateTime->tm_min, dateTime->tm_sec, milliseconds,
        description);

    if(fputs(logMsg, logFile) == EOF) {
        char msg[160];
        sprintf(msg, "logger - writing log file %s", description);
        perror(msg);
        return -1;
    }
    fflush(logFile);
}
