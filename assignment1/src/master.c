#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/inotify.h>
#include <poll.h>
#include <string.h>
#include <ctype.h>
#include "./helpers/logger.h"

/*--------------------------------------------------------------------
                            GLOVAL VARIABLES
--------------------------------------------------------------------*/
int terminateChildren = 0;
const unsigned short NUM_LOG_FILE = 6;
FILE *logFiles[6];
char msg[128];  // 

/*--------------------------------------------------------------------
                            SERVICE FUNCTIONS
--------------------------------------------------------------------*/

int spawn(const char *program, char **arg_list)
{
    pid_t child_pid = fork();
    if (child_pid == 0)
    {
        snprintf(msg, sizeof(msg), "master - executing program %s\n", program);
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        execvp(program, arg_list);
        snprintf(msg, sizeof(msg), "master - failed in execvp program %s\n", program);
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
    }

    if (child_pid > 0)
    {
        return child_pid;
    }

    if (child_pid < 0)
    {
        snprintf(msg, sizeof(msg), "master - fork failed");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        return EXIT_FAILURE;
    }
}

int closePipes(int pipe[2])
{
    int rclose;
    for (unsigned short idx = 0; idx < 2; idx++)
    {
        rclose = close(pipe[idx]);
        if (rclose == -1)
            return rclose;
    }
    return rclose;
}

/* Handler for the stop signal (SIGTERM)*/
void termHandler(int signo) {
    if (writeLogMsg(logFiles[5], "master - SIGTERM received\n") == -1)
        exit(errno);
    signal(SIGTERM, termHandler);

    terminateChildren = 1;
}


int main(int argc, char *argv[])
{
    // listener to SIGTERM for closing
    if (signal(SIGTERM, termHandler) == SIG_ERR)
    {
        sprintf(msg, "master - SIGTERM not handled\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
        exit(errno);
    }
    
    int cmdlineOpt;
    int watchdog_timeout = 60000;   // default value 60s
    int samplingTime = 1000;        // default sampling time
    char noiseAmplitude_send[8] = "0.4";

    // parsing input options
    while ((cmdlineOpt = getopt(argc, argv, "t:s:n:")) != -1)
    {
        switch (cmdlineOpt)
        {
            case 't':
                watchdog_timeout = atoi(optarg) * 1000;
                watchdog_timeout = (watchdog_timeout < 2000) ? 2000 : watchdog_timeout;
                break;

            case 's':
                samplingTime = atoi(optarg);
                // set minimum sampling time to 30ms
                samplingTime = (samplingTime < 30) ? 30 : samplingTime;
                break;

            case 'n':
                // sscanf(optarg, "%f", &noiseAmplitude);
                strcpy(noiseAmplitude_send, optarg);
                break;

            case '?':
                if (optopt == 't')
                    fprintf(stderr, "Option t requires timeout in seconds (integer)\n");
                else if (optopt == 's')
                    fprintf(stderr, "Option s requires sampling time in milliseconds (integer)\n");
                else if (optopt == 'n')
                    fprintf(stderr, "Option n requires the amplitude around zero of the noise (float)\n");
                else if (isprint(optopt))
                    fprintf(stderr, "Unknown option character -%c\n", optopt);
                return 1;
            default:
                abort();
        }
    }
    /*------------------------------------------------------
                create log files for each task
    -------------------------------------------------------*/
    char logdir[] = "./logs";
    int res_mkdir = mkdir(logdir, umask(0777));
    char cmdConsoleLog_path[] = "./logs/cmdConsole.log";
    char inspConsoleLog_path[] = "./logs/inspConsole.log";
    char motorXLog_path[] = "./logs/motorX.log";
    char motorZLog_path[] = "./logs/motorZ.log";
    char worldLog_path[] = "./logs/world.log";
    char masterLog_path[] = "./logs/master.log";

    if (res_mkdir < 0){
        if (errno == EEXIST)
        {
            printf("warning - log directory already exists. New log entries will be appended to the existing files\n");
        }
        else {
            perror("master - creating log files directory");
            exit(EXIT_FAILURE);
        }
    }
    char openFlag[] = "a";

    /*----------------- COMMAND CONSOLE -----------------*/
    FILE *cmdConsoleLog = fopen(cmdConsoleLog_path, openFlag);
    if (cmdConsoleLog == NULL) {
        perror("master - creating command console log file");
        exit(errno);
    }
    logFiles[0] = cmdConsoleLog;
    // get associated file descriptor to pass to the corresponding process
    int fdcmdConsoleLog = fileno(cmdConsoleLog);
    if (fdcmdConsoleLog == -1) {
        perror("master - getting file descriptor for command console log file");
        exit(errno);
    }
    // convert it to string for send it to the related process
    char fdcmdConsoleLog_send[16];
    snprintf(fdcmdConsoleLog_send, sizeof(fdcmdConsoleLog_send),
        "%d", fdcmdConsoleLog);

    /*--------------- INSPECTION CONSOLE ----------------*/
    FILE *inspConsoleLog = fopen(inspConsoleLog_path, openFlag);
    if (inspConsoleLog == NULL) {
        perror("master - creating inspection console log file");
        exit(errno);
    }
    logFiles[1] = inspConsoleLog;
    // convert get associated file descriptor to pass to the corresponding process
    int fdinspConsoleLog = fileno(inspConsoleLog);
    if (fdinspConsoleLog == -1) {
        perror("master - getting file descriptor for inspection console log file");
        exit(errno);
    }
    // convert it to string for send it to the related process
    char fdinspConsoleLog_send[16];
    snprintf(fdinspConsoleLog_send, sizeof(fdinspConsoleLog_send),
        "%d", fdinspConsoleLog);
    
    /*--------------------- MOTOR X --------------------*/
    FILE *motorXLog = fopen(motorXLog_path, openFlag);
    if (motorXLog == NULL) {
        perror("master - creating motor X log file");
        exit(errno);
    }
    logFiles[2] = motorXLog;
    // convert get associated file descriptor to pass to the corresponding process
    int fdmotorXLog = fileno(motorXLog);
    if (fdmotorXLog == -1) {
        perror("master - getting file descriptor for motor X log file");
        exit(errno);
    }
    // convert it to string for send it to the related process
    char fdmotorXLog_send[16];
    snprintf(fdmotorXLog_send, sizeof(fdmotorXLog_send),
        "%d", fdmotorXLog);

    /*--------------------- MOTOR Z --------------------*/
    FILE *motorZLog = fopen(motorZLog_path, openFlag);
    if (motorZLog == NULL) {
        perror("master - creating motor Z log file");
        exit(errno);
    }
    logFiles[3] = motorZLog;
    // convert get associated file descriptor to pass to the corresponding process
    int fdmotorZLog = fileno(motorZLog);
    if (fdmotorZLog == -1) {
        perror("master - getting file descriptor for motor Z log file");
        exit(errno);
    }
    // convert it to string for send it to the related process
    char fdmotorZLog_send[16];
    snprintf(fdmotorZLog_send, sizeof(fdmotorZLog_send),
        "%d", fdmotorZLog);

    /*----------------------- WORLD ----------------------*/
    FILE *worldLog = fopen(worldLog_path, openFlag);
    if (worldLog == NULL) {
        perror("master - creating world log file");
        exit(errno);
    }
    logFiles[4] = worldLog;
    // convert get associated file descriptor to pass to the corresponding process
    int fdworldLog = fileno(worldLog);
    if (fdworldLog == -1) {
        perror("master - getting file descriptor for world log file");
        exit(errno);
    }
    // convert it to string for send it to the related process
    char fdworldLog_send[16];
    snprintf(fdworldLog_send, sizeof(fdworldLog_send),
        "%d", fdworldLog);

    /*---------------------- MASTER ----------------------*/
    FILE *masterLog = fopen(masterLog_path, openFlag);
    if (masterLog == NULL) {
        perror("master - creating master log file");
        exit(errno);
    }
    logFiles[5] = masterLog;

    /*------------------------------------------------------
                        start master logger
    -------------------------------------------------------*/
    snprintf(msg, sizeof(msg), "************** NEW MASTER INSTANCE **************\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "master - watchdog timeout: %dms\n", watchdog_timeout);
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "master - sampling time: %dms\n", samplingTime);
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    snprintf(msg, sizeof(msg), "master - noise amplitude: %s\n", noiseAmplitude_send);
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);

    // convert sampling time for sending it to processes
    char samplingTime_send[16];
    snprintf(samplingTime_send, sizeof(samplingTime_send), "%d", samplingTime);
    
    // get master process id for providing it to
    // command console for sending terminate command
    pid_t pid_master = getpid();
    // convert master pid to string
    char pid_master_send[16];
    snprintf(pid_master_send, sizeof(pid_master_send),
        "%d", pid_master);

    /*------------------------------------------------------
                        motor parameters
    -------------------------------------------------------*/
    /*--------------------- MOTOR X --------------------*/
    char motorNameX[] = "motorX";
    snprintf(msg, sizeof(msg), "master - motor X name: %s\n", motorNameX);
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);

    float baseVelocityX = 1.0;
    snprintf(msg, sizeof(msg), "master - base velocity X: %f\n", baseVelocityX);
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    // convert to char* for sending it as parameter
    char baseVelocityX_send[64];
    snprintf(baseVelocityX_send, sizeof(baseVelocityX_send),
        "%f",baseVelocityX);
    
    // lower limit
    float xmin = 0.0;
    snprintf(msg, sizeof(msg), "master - motor X min position: %f\n", xmin);
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    // convert to char* for sending it as parameter
    char xmin_send[64];
    snprintf(xmin_send, sizeof(xmin_send),
        "%f", xmin);

    // upper limit
    float xmax = 40.0;
    snprintf(msg, sizeof(msg), "master - motor X max position: %f\n", xmax);
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    // convert to char* for sending it as parameter
    char xmax_send[64];
    snprintf(xmax_send, sizeof(xmax_send),
        "%f", xmax);


    /*--------------------- MOTOR Z --------------------*/
    char motorNameZ[] = "motorZ";
    snprintf(msg, sizeof(msg), "master - motor Z name: %s\n", motorNameZ);
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);

    float baseVelocityZ = baseVelocityX / 4;
    snprintf(msg, sizeof(msg), "master - base velocity Z: %f\n", baseVelocityZ);
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    // convert to char* for sending it as parameter
    char baseVelocityZ_send[64];
    snprintf(baseVelocityZ_send, sizeof(baseVelocityZ_send),
        "%f",baseVelocityZ);
    
    // lower limit
    float zmin = 0.0;
    snprintf(msg, sizeof(msg), "master - motor Z min position: %f\n", zmin);
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    // convert to char* for sending it as parameter
    char zmin_send[64];
    snprintf(zmin_send, sizeof(zmin_send),
        "%f", zmin);
    
    // upper limit
    float zmax = 10.0;
    snprintf(msg, sizeof(msg), "master - motor Z max position: %f\n", zmax);
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    // convert to char* for sending it as parameter
    char zmax_send[64];
    snprintf(zmax_send, sizeof(zmax_send),
        "%f", zmax);

    /*------------------------------------------------------
                        create pipes
    -------------------------------------------------------*/
    /*
        command console:
        -> motor x
        -> motor z
    */
    int fdPipe_cmd_mx[2];
    if (pipe(fdPipe_cmd_mx) < 0){
        snprintf(msg, sizeof(msg), "master - failed creating pipe command to motor X\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
        exit(EXIT_FAILURE);
    }
    snprintf(msg, sizeof(msg), "master - created pipe command console to motor X\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    // convert pipes to char for sending them as arguments
    // for exec
    char fdPipe_cmd_mx_read[16];
    char fdPipe_cmd_mx_write[16];
    snprintf(fdPipe_cmd_mx_read, sizeof(fdPipe_cmd_mx_read),
        "%d", fdPipe_cmd_mx[0]);
    snprintf(fdPipe_cmd_mx_write, sizeof(fdPipe_cmd_mx_write),
        "%d", fdPipe_cmd_mx[1]);

    int fdPipe_cmd_mz[2];
    if (pipe(fdPipe_cmd_mz) < 0){
        snprintf(msg, sizeof(msg), "master - failed creating pipe command to motor Z\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
        exit(EXIT_FAILURE);
    }
    snprintf(msg, sizeof(msg), "master - created pipe command console to motor Z\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);

    // convert pipes to char for sending them as arguments
    // for exec
    char fdPipe_cmd_mz_read[16];
    char fdPipe_cmd_mz_write[16];
    snprintf(fdPipe_cmd_mz_read, sizeof(fdPipe_cmd_mz_read),
        "%d", fdPipe_cmd_mz[0]);
    snprintf(fdPipe_cmd_mz_write, sizeof(fdPipe_cmd_mz_write),
        "%d", fdPipe_cmd_mz[1]);

    /*
        motor x -> world
    */
    int fdPipe_mx_world[2];
    if (pipe(fdPipe_mx_world) < 0){
        snprintf(msg, sizeof(msg), "master - failed creating pipe motor X to world\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
        exit(EXIT_FAILURE);
    }
    snprintf(msg, sizeof(msg), "master - created pipe pipe motor X to world\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    
    // convert pipes to char for sending them as arguments
    // for exec
    char fdPipe_mx_world_read[16];
    char fdPipe_mx_world_write[16];
    snprintf(fdPipe_mx_world_read, sizeof(fdPipe_mx_world_read),
        "%d", fdPipe_mx_world[0]);
    snprintf(fdPipe_mx_world_write, sizeof(fdPipe_mx_world_write),
        "%d", fdPipe_mx_world[1]);
    
    /*
        motor z -> world
    */
    int fdPipe_mz_world[2];
    if (pipe(fdPipe_mz_world) < 0){
        snprintf(msg, sizeof(msg), "master - failed creating pipe motor Z to world\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
        exit(EXIT_FAILURE);
    }

    snprintf(msg, sizeof(msg), "master - created pipe motor Z to world\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);

    // convert pipes to char for sending them as arguments
    // for exec
    char fdPipe_mz_world_read[16];
    char fdPipe_mz_world_write[16];
    snprintf(fdPipe_mz_world_read, sizeof(fdPipe_mz_world_read),
        "%d", fdPipe_mz_world[0]);
    snprintf(fdPipe_mz_world_write, sizeof(fdPipe_mz_world_write),
        "%d", fdPipe_mz_world[1]);

    /*
        world -> inspection
    */
    int fdPipe_world_insp[2];
    if (pipe(fdPipe_world_insp) < 0){
        snprintf(msg, sizeof(msg), "master - failed creating pipe world to inspection console\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
        exit(EXIT_FAILURE);
    }

    snprintf(msg, sizeof(msg), "master - created pipe world to inspection console\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);

    // convert pipes to char for sending them as arguments
    // for exec
    char fdPipe_world_insp_read[16];
    char fdPipe_world_insp_write[16];
    snprintf(fdPipe_world_insp_read, sizeof(fdPipe_world_insp_read),
        "%d", fdPipe_world_insp[0]);
    snprintf(fdPipe_world_insp_write, sizeof(fdPipe_world_insp_write),
        "%d", fdPipe_world_insp[1]);

    /*------------------------------------------------------
                    spawn child tasks
    -------------------------------------------------------*/
    /*--------------------- MOTOR X --------------------*/
    char *arg_list_motorX[] = {"./bin/motor", &baseVelocityX_send[0], &xmin_send[0], &xmax_send[0],
        &fdPipe_cmd_mx_read[0], &fdPipe_mx_world_write[0], fdmotorXLog_send, motorNameX, samplingTime_send, NULL};
    pid_t pid_motorX = spawn("./bin/motor", arg_list_motorX);
    
    snprintf(msg, sizeof(msg), "master - spawned motor X\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    
    // convert pid to char for sending them as arguments
    // for inspection console
    char pid_motorX_send[16];
    snprintf(pid_motorX_send, sizeof(pid_motorX_send),
        "%d", pid_motorX);

    /*--------------------- MOTOR Z --------------------*/
    char *arg_list_motorZ[] = {"./bin/motor", &baseVelocityZ_send[0], &zmin_send[0], &zmax_send[0],
        &fdPipe_cmd_mz_read[0], &fdPipe_mz_world_write[0], fdmotorZLog_send, motorNameZ, samplingTime_send, NULL};
    pid_t pid_motorZ = spawn("./bin/motor", arg_list_motorZ);

    snprintf(msg, sizeof(msg), "master - spawned motor Z\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    
    // convert pid to char for sending them as arguments
    // for inspection console
    char pid_motorZ_send[16];
    snprintf(pid_motorZ_send, sizeof(pid_motorZ_send),
        "%d", pid_motorZ);

    /*----------------- COMMAND CONSOLE -----------------*/
    char *arg_list_command[] = {"/usr/bin/konsole", "--hold", "-e", "./bin/command",
        &fdPipe_cmd_mx_write[0], &fdPipe_cmd_mz_write[0], pid_master_send, fdcmdConsoleLog_send, NULL};
    pid_t pid_cmd = spawn("/usr/bin/konsole", arg_list_command);

    snprintf(msg, sizeof(msg), "master - spawned command console\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);

    /*--------------- INSPECTION CONSOLE ----------------*/
    char *arg_list_inspection[] = {"/usr/bin/konsole", "--hold", "-e", "./bin/inspection",
        &fdPipe_world_insp_read[0], pid_motorX_send, pid_motorZ_send, fdinspConsoleLog_send, samplingTime_send, NULL};
    pid_t pid_insp = spawn("/usr/bin/konsole", arg_list_inspection);

    snprintf(msg, sizeof(msg), "master - spawned inspection console\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);

    /*----------------------- WORLD ----------------------*/
    char *arg_list_world[] = {"./bin/world", &fdPipe_mx_world_read[0],
        &fdPipe_mz_world_read[0], &fdPipe_world_insp_write[0], fdworldLog_send, samplingTime_send, noiseAmplitude_send, NULL};
    pid_t pid_world = spawn("./bin/world", arg_list_world);

    snprintf(msg, sizeof(msg), "master - spawned world\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);

    /*------------------------------------------------------
                            watchdog
    -------------------------------------------------------*/
    // Once expired it will kill all child process
    
    // create inotify structure for watching log directory
    int watchLogFiles_fd = inotify_init1(0);
    if (watchLogFiles_fd == -1) {
        snprintf(msg, sizeof(msg), "master - failed on inotify initialization\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
        exit(errno);
    }
    int watchLogFiles_wd = inotify_add_watch(watchLogFiles_fd, logdir, IN_MODIFY);
    if (watchLogFiles_wd == -1){
        snprintf(msg, sizeof(msg), "master - failed on adding watcher initialization\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
        exit(errno);
    }

    // poll structure for reading watch log file events
    struct pollfd poll_watchLogfile = {
        .fd = watchLogFiles_fd,
        .events = POLLIN
    };

    while (!terminateChildren)
    {
        int res = poll(&poll_watchLogfile, 1, watchdog_timeout);
        // error handling
            if (res == -1) {
                // if not because of an incoming signal
                if (errno != EINTR) {
                    
                    snprintf(msg, sizeof(msg), "master - failed on polling watchdog\n");
                    if (writeLogMsg(logFiles[5], msg) == -1)
                        exit(errno);
                    perror(msg);
                    exit(errno);
                }
            }

            // Timeout
            else if (res == 0) {
                snprintf(msg, sizeof(msg), "************* WATCHDOG TIMEOUT *************\n");
                if (writeLogMsg(logFiles[5], msg) == -1)
                    exit(errno);
                break;
            }

            else {
                const int BUF_LEN = 16*sizeof(struct inotify_event);
                char buf[BUF_LEN];
                int len;
                
                len = read(watchLogFiles_fd, buf, BUF_LEN);

                if (len == -1) {
                    snprintf(msg, sizeof(msg), "master - failed reading inotify event\n");
                    if (writeLogMsg(logFiles[5], msg) == -1)
                        exit(errno);
                    perror(msg);
                    exit(errno);
                }
            }
    }

    snprintf(msg, sizeof(msg), "master - terminating all child processes\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);

    if (kill(pid_motorX, SIGTERM) == -1) {
        snprintf(msg, sizeof(msg), "master - failed on sending SIGTERM to motor X\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
    }
    snprintf(msg, sizeof(msg), "master - SIGTERM signal sent to motor X\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);

    if (kill(pid_motorZ, SIGTERM) == -1) {
        snprintf(msg, sizeof(msg), "master - failed on sending SIGTERM to motor Z\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
    }
    snprintf(msg, sizeof(msg), "master - SIGTERM signal sent to motor Z\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);

    if (kill(pid_world, SIGTERM) == -1){
        snprintf(msg, sizeof(msg), "master - failed on sending SIGTERM to world\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
    }
    snprintf(msg, sizeof(msg), "master - SIGTERM signal sent to world\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);

    if (kill(pid_cmd, SIGTERM)){
        snprintf(msg, sizeof(msg), "master - failed on sending SIGTERM to command console\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
    }
    snprintf(msg, sizeof(msg), "master - SIGTERM signal sent to command console\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);

    if (kill(pid_insp, SIGTERM)){
        snprintf(msg, sizeof(msg), "master - failed on sending SIGTERM to inspection console\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
    }
    snprintf(msg, sizeof(msg), "master - SIGTERM signal sent to inspection console\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    
    /*------------------------------------------------------
                waiting for child tasks to end
    -------------------------------------------------------*/
    int status;
    gid_t gid = getpgid(0);
    unsigned short numTask = 5;
    snprintf(msg, sizeof(msg), "waiting for processes to terminate\n");
    if (writeLogMsg(logFiles[5], msg) == -1)
        exit(errno);
    
    for (unsigned short i = 0; i < numTask; i++)
    {
        pid_t processPid = waitpid(-gid, &status, 0);

        if (processPid == pid_cmd)
        {
            snprintf(msg, sizeof(msg), "Command window terminated with status %s\n", strsignal(status));
            if (writeLogMsg(logFiles[5], msg) == -1)
                exit(errno);
            
        }
        else if (processPid == pid_insp)
        {
            snprintf(msg, sizeof(msg), "Inspection window terminated with status %s\n", strsignal(status));
            if (writeLogMsg(logFiles[5], msg) == -1)
                exit(errno);
        }
        else if (processPid == pid_motorX)
        {
            snprintf(msg, sizeof(msg), "Motor X terminated with status %s\n", strsignal(status));
            if (writeLogMsg(logFiles[5], msg) == -1)
                exit(errno);
        }
        else if (processPid == pid_motorZ)
        {
            snprintf(msg, sizeof(msg), "Motor Z terminated with status %s\n", strsignal(status));
            if (writeLogMsg(logFiles[5], msg) == -1)
                exit(errno);
        }
        else if (processPid == pid_world)
        {
            snprintf(msg, sizeof(msg), "World terminated with status %s\n", strsignal(status));
            if (writeLogMsg(logFiles[5], msg) == -1)
                exit(errno);
        }
        else if (processPid == -1)
        {
            snprintf(msg, sizeof(msg), "master - error waitpid");
            if (writeLogMsg(logFiles[5], msg) == -1)
                exit(errno);
        }
    }

    /*-------------------------------------------------
                    close pipes
    -------------------------------------------------*/
    int closePipesError = 0;
    if (closePipes(fdPipe_cmd_mx) == -1)
    {
        closePipesError = -1;
        snprintf(msg, sizeof(msg), "master - error closing pipe command console to motor X\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
    }
    if (closePipes(fdPipe_cmd_mz) == -1)
    {
        closePipesError = -1;
        snprintf(msg, sizeof(msg), "master - error closing pipe command console to motor Z\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
    }
    if (closePipes(fdPipe_mx_world) == -1)
    {
        closePipesError = -1;
        snprintf(msg, sizeof(msg), "master - error closing pipe motor X to world\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
    }
    if (closePipes(fdPipe_mz_world) == -1)
    {
        closePipesError = -1;
        snprintf(msg, sizeof(msg), "master - error closing pipe motor z to world\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
    }
    if (closePipes(fdPipe_world_insp) == -1)
    {
        closePipesError = -1;
        snprintf(msg, sizeof(msg), "master - error closing pipe world to inspection console\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        perror(msg);
    }
    if (closePipesError == -1) {
        exit(EXIT_FAILURE);
    }

    /*-------------------------------------------------
                    close log files
    -------------------------------------------------*/
    // do not remove them for keeping information even after
    // the process is done
    int rclose;
    int logFileError = 0;
    for (unsigned short idx = 0; idx < NUM_LOG_FILE; idx++)
    {
        rclose = fclose(logFiles[idx]);
        if (rclose)
        {
            perror("closing log files");
            logFileError = 1;
        }
    }

    if (logFileError == 1) {
        snprintf(msg, sizeof(msg), "master - error closing log files\n");
        if (writeLogMsg(logFiles[5], msg) == -1)
            exit(errno);
        exit(errno);
    }

    printf("Master process exiting correctly\n");
    return 0;
}
