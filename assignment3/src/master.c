#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <errno.h>
#include <sys/mman.h>   // for shared memory
#include <sys/stat.h>   // for mode constants
#include <fcntl.h>      // for O_ constants
#include <semaphore.h>
#include <string.h>
#include "./../include/helper.h"

/*--------------------------------------------------------------------
							GLOBAL VARIABLES
--------------------------------------------------------------------*/
pid_t pid_procA = 0;
pid_t pid_procB = 0;
pid_t pid_konsoleA, pid_konsoleB;
int count = 0;

/*--------------------------------------------------------------------
							SERVICE FUNCTIONS
--------------------------------------------------------------------*/
int spawn(const char *program, char **arg_list)
{
    char msg[128];
	pid_t child_pid = fork();
    if (child_pid == 0)
    {
        execvp(program, arg_list);
        snprintf(msg, sizeof(msg), "master - failed on execvp program %s", program);
        perror(msg);
		return EXIT_FAILURE;
    }

    if (child_pid > 0)
    {
        return child_pid;
    }

    if (child_pid < 0)
    {
        snprintf(msg, sizeof(msg), "master - failed on fork program %s", program);
        perror(msg);
        return EXIT_FAILURE;
    }
}

// Handler for SIGUSR1 for process A pid
void usr1Handler(int signo, siginfo_t *si, void *unused)
{

	// printf("master received USR1 signal\n");
	// fflush(stdout);
	
	if (si->si_int < 0) {
		// printf("killing konsole A\n");
		// fflush(stdout);
		// process A closed properly
		// it is possible to close konsole A
		if (kill(pid_konsoleA, SIGTERM)) 
		{
			perror("master - failed kill SIGTERM to konsole A");
			exit(EXIT_FAILURE);
		}
	} else {
		// register the process A pid
		pid_procA = (pid_t)si->si_int;
		// printf("process A pid: %d\n", pid_procA);
		// fflush(stdout);
	}
}

// Handler for SIGUSR2 for process B pid
void usr2Handler(int signo, siginfo_t *si, void *unused)
{
	if (si->si_int < 0) {
		// printf("killing konsole B\n");
		// fflush(stdout);
		// process B closed properly
		// it is possible to close konsole B
		if (kill(pid_konsoleB, SIGTERM)) 
		{
			perror("master - failed kill SIGTERM to konsole B");
			exit(EXIT_FAILURE);
		}
	} else {
		// register the process A pid
		pid_procB = (pid_t)si->si_int;
		// printf("process B pid: %d\n", pid_procB);
		// fflush(stdout);
	}
}

void termHandler(int signo) {
	// printf("master - SIGTERM received\n");
	// fflush(stdout);
	
	if (signal(SIGTERM, termHandler) == SIG_ERR)
    {
        perror("master - failed signal handler fro SIGTERM");
        exit(EXIT_FAILURE);
    }

	// close process B
	if (kill(pid_procB, SIGTERM) == -1)
	{
		perror("master - failed kill SIGTERM to process B");
		exit(EXIT_FAILURE);
	}
}

/*--------------------------------------------------------------------
								MAIN
--------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
	/*-------------------------------------------------------------------
							INPUT ARGUMENTS
	--------------------------------------------------------------------*/
	int cmdlineOpt;
	char server[32], portno[8], mode[4];
	memset(server, 0, sizeof(server));
	memset(portno, 0, sizeof(portno));
	memset(mode, 0, sizeof(mode));
	strncpy(mode, "s", 2);
	while ((cmdlineOpt = getopt(argc, argv, "a:p:c")) != -1)
	{
		switch(cmdlineOpt)
		{
			case 'a':
				strncpy(server, optarg, strlen(optarg)+1);
				break;

			case 'p':
				strncpy(portno, optarg, strlen(optarg)+1);
				break;
			
			case 'c':
				strncpy(mode, "c", 2);
				break;
			
			case '?':
				if (optopt == 'a')
					fprintf(stderr, "Option a requires a valid host name or address");
				else if (optopt == 'p')
					fprintf(stderr, "Option p requires a valid port number");
				return EXIT_FAILURE;
			
			default:
				abort();
		}
	}
	
	int isServer = 1 && strncmp(mode, "c", 1);

	/*-------------------------------------------------------------------
							SIGNAL LISTENERS
	--------------------------------------------------------------------*/
	struct sigaction sa1,sa2;

	// structure for USR1
	sa1.sa_flags = SA_SIGINFO;
	sigemptyset(&sa1.sa_mask);
	sa1.sa_sigaction = usr1Handler;

	// structure for USR2
	sa2.sa_flags = SA_SIGINFO;
	sigemptyset(&sa2.sa_mask);
	sa2.sa_sigaction = usr2Handler;

	// listener to SIGUSR1 for process A pid
    if (sigaction(SIGUSR1, &sa1, NULL) == -1) {
		perror("master - failed sigaction handler for SIGUSR1");
		exit(EXIT_FAILURE);
	}

	// listener to SIGUSR2 for process A pid
    if (sigaction(SIGUSR2, &sa2, NULL) == -1) {
		perror("master - failed sigaction handler for SIGUSR2");
		exit(EXIT_FAILURE);
	}
	
	// listener to SIGTERM for closing
	if (signal(SIGTERM, termHandler) == SIG_ERR)
    {
        perror("master - failed signal handler fro SIGTERM");
        exit(EXIT_FAILURE);
    }

	/*--------------------------------------------------------------------
							SHARED MEMEORY
	--------------------------------------------------------------------*/
	PICTURE *pSharedPicture;
	size_t sharedMem_size = sizeof(pSharedPicture);
    int sharedMem_fd;
	char sharedMem_name[32];
	setName(SHARED_MEM_NAME, isServer, sharedMem_name);
    sharedMem_fd = shm_open(sharedMem_name, O_CREAT | O_RDWR, 0666);
    if (sharedMem_fd == -1) {
        perror("process A -shm_open failed");
        exit(EXIT_FAILURE);
    }
    ftruncate(sharedMem_fd, sharedMem_size);
    
	/*--------------------------------------------------------------------
							SHARED SEMAPHORES
	--------------------------------------------------------------------*/
	// create semaphore for writing
	char semaphore_write_name[32];
	setName(SEMAPHORE_WRITE, isServer, semaphore_write_name);
    sem_t *pSemaphore_write =sem_open(semaphore_write_name, O_CREAT | O_RDWR, 0666, 1);
    if (pSemaphore_write == SEM_FAILED) {
        perror("master - failed sem_open on pSemaphore_write");
        exit(EXIT_FAILURE);
    }
    // initialize semaphore for writing - writing enabled
    if (sem_init(pSemaphore_write, 1, 1) == -1) {
        perror("master - failed sem_init on pSemaphore_write");
        exit(EXIT_FAILURE);
    }

    // createe semaphore for reading
	char semaphore_read_name[32];
	setName(SEMAPHORE_READ, isServer, semaphore_read_name);
    sem_t *pSemaphore_read = sem_open(semaphore_read_name, O_CREAT | O_RDWR, 0666, 0);
    if (pSemaphore_read == SEM_FAILED) {
        perror("master - failed sem_open on pSemaphore_read");
        exit(EXIT_FAILURE);
    }
    // initialize semaphore for reading - reading disabled
    if (sem_init(pSemaphore_read, 1, 0) == -1) {
        perror("master - failed sem_init on pSemaphore_write");
        exit(EXIT_FAILURE);
    }

	/*--------------------------------------------------------------------
						FILE NAME FOR PRINTING
	--------------------------------------------------------------------*/
	/*-------------------------------
		create output directory
	-------------------------------*/
	char *outdir = "./out";
	int res_mkdir = mkdir(outdir, umask(0777));
	if (res_mkdir < 0){
        if (errno != EEXIST) // skip error if output directory already existes
        {
            perror("master - creating output directory for pictures");
            exit(EXIT_FAILURE);
        }
    }
	char *fileName = "circle";
	char *fileExstension = ".bmp";

	/*--------------------------------------------------------------------
							PROCESS SPAWNING
	--------------------------------------------------------------------*/
	/* state machine to guarantee that process A starts and initializes
	   before process B
	*/
	enum {
		SPAWN_A,
		WAIT_FOR_PROC_A,
		SPAWN_B,
		WAIT_FOR_PROC_B,
		INIT_DONE
	} master_state;

	// get master pid
	pid_t pid_master = getpid();
	
	// convert master pid to string
	char pid_master_send[16];
	snprintf(pid_master_send, sizeof(pid_master_send),
			 "%d", pid_master);

	char processA[32];
	memset(processA, 0, strlen(processA));
	if (isServer == 0) {
		strncpy(processA, "./bin/processA_client", strlen("./bin/processA_client")+1);
	} else {
		strncpy(processA, "./bin/processA_server", strlen("./bin/processA_server")+1);
	}
	char *arg_list_A[] = {"/usr/bin/konsole", "--hold", "-e", processA,
	pid_master_send, outdir, fileName, fileExstension, server, portno, NULL};
	char *arg_list_B[] = {"/usr/bin/konsole", "--hold", "-e", "./bin/processB",
	pid_master_send, mode, NULL};

	master_state = SPAWN_A;
	int init_done = 0;

	while(init_done == 0) {

		switch(master_state)
		{
		case SPAWN_A:
			// printf("state SPAWN_A\n");
			// fflush(stdout);
			
			pid_konsoleA = spawn("/usr/bin/konsole", arg_list_A);
			if (pid_konsoleA == -1) {
				exit(EXIT_FAILURE);
			}
			master_state = WAIT_FOR_PROC_A;
			break;
		
		case WAIT_FOR_PROC_A:
		// process A will send its pid once its initialization
		// is finished
			if (pid_procA > 0) {
				// printf("moving to SPAWN_B\n");
				// fflush(stdout);
				master_state = SPAWN_B;
			}
			break;

		case SPAWN_B:
			// printf("state SPAWN_B\n");
			// fflush(stdout);
			pid_konsoleB = spawn("/usr/bin/konsole", arg_list_B);
			if (pid_konsoleB == -1) {
				exit(EXIT_FAILURE);
			}
			master_state = WAIT_FOR_PROC_B;
			break;

		case WAIT_FOR_PROC_B:
		// process B will send its pid once its initialization
		// is finished 
			if (pid_procB > 0) {
				// printf("moving to INIT_DONE\n");
				// fflush(stdout);
				master_state = INIT_DONE;
			}
			break;

		case INIT_DONE:
			// printf("state WORKING\n");
			// fflush(stdout);

			init_done = 1;
			break;

		default:
			printf("master - Unexpected initialization state\n");
			fflush(stdout);
			exit(EXIT_FAILURE);
			break;
		}
	}

	/*--------------------------------------------------------------------
								PROCESSES CLOSING
	--------------------------------------------------------------------*/

	// wait for process A to send SIGUSR1 for having
	// closed correctly
	sigset_t sset;
	sigfillset(&sset);
	sigdelset(&sset, SIGUSR1);
	if(sigsuspend(&sset) == -1) {
		if (errno != EINTR) {
			perror("master - failed sigsuspend on SIGUSR1");
		}
	}

	// close process B
	if (kill(pid_procB, SIGTERM) == -1)
	{
		perror("master - failed kill SIGTERM to process B");
		exit(EXIT_FAILURE);
	}

	// wait for process B to send SIGUSR2 for having
	// closed correctly
	sigfillset(&sset);
	sigdelset(&sset, SIGUSR2);
	if(sigsuspend(&sset) == -1) {
		if (errno != EINTR) {
			perror("master - failed sigsuspend on SIGUSR2");
		}
	}

	int status_konsoleA, status_konsoleB;
	// wait for konsole A to close
	if (waitpid(pid_konsoleA, &status_konsoleA, 0) == -1){
		perror("master - failed waitpid on konsole A");
		exit(EXIT_FAILURE);
	}

	// wait for konsole B to close
	if (waitpid(pid_konsoleB, &status_konsoleB, 0) == -1){
		perror("master - failed waitpid on konsole B");
		exit(EXIT_FAILURE);
	}

	/*----------------------------
        shared memory clean up
    ----------------------------*/
	if (shm_unlink(sharedMem_name) == -1) {
        perror("master - failed shm_unlink");
        exit(EXIT_FAILURE);
    }

	/*-------------------------------
        shared semaphores clean up
    -------------------------------*/
	if (sem_close(pSemaphore_write) == -1) {
        perror("master - failed sem_close pSemaphore_write");
        exit(EXIT_FAILURE);
    }
    if (sem_close(pSemaphore_read) == -1) {
        perror("master - failed sem_close pSemaphore_read");
        exit(EXIT_FAILURE);
    }
	if (sem_unlink(semaphore_write_name) == -1) {
        perror("master - failed sem_unlink pSemaphore_write");
        exit(EXIT_FAILURE);
    }
    if (sem_unlink(semaphore_read_name) == -1) {
        perror("master - failed sem_unlink pSemaphore_read");
        exit(EXIT_FAILURE);
    }

	printf("Main program exiting\n" \
		"konsole A exited with status: %d\n" \
		"konsole B exited with status: %d\n",
		status_konsoleA,status_konsoleB);
	
	return 0;
}
