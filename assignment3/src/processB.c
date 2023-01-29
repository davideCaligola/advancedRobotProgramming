#include <sys/signal.h>
#include <sys/mman.h>   // for shared memory
#include <sys/stat.h>   // for mode constants
#include <fcntl.h>      // for O_ constants
#include <errno.h>
#include <semaphore.h>
#include <math.h>
#include "./../include/processB_utilities.h"
#include "./../include/helper.h"

typedef struct {
    int x,y;
} Point2D;

typedef struct {
    int xmin, xmax, ymin, ymax;
} CircleLimits;

/*--------------------------------------------------------------------
							GLOBAL VARIABLES
--------------------------------------------------------------------*/
int terminate = 0;
sem_t *pSemaphore_write;
sem_t *pSemaphore_read;

/*--------------------------------------------------------------------
							SERVICE FUNCTIONS
--------------------------------------------------------------------*/
// copy shared picture to local variable
void copySharedPicture(PICTURE* pSharedPicture, PICTURE *pLocalPicture);

// estimate the center of a full circle in picture
void getCenter(PICTURE *picture, Point2D *center);

// updates the current limit of the circle
// given the current point withing the circle
void updateMinMax(int x,int y, CircleLimits *currLimits);

// update center position in window GUI
int updateCenterPosition(PICTURE *pSharedPicture,
    sem_t *pSemaphore_read, sem_t *pSemaphore_write,
    float scaleFactorX, float scaleFactorY);

/* Handler for the stop signal (SIGTERM)*/
void termHandler(int signo)
{
    if (signal(SIGTERM, termHandler) == SIG_ERR)
    {
        perror("processB - failed signal handler fro SIGTERM");
        exit(EXIT_FAILURE);
    }
    
    // exit working loop
    terminate = 1;

    // unlock reading semaphore for closing process
    if (sem_post(pSemaphore_read) == -1) {
        perror("process B - failed sem_post on pSemaphore_read");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char const *argv[])
{
    /*-------------------
        signal listeners
    --------------------*/

    // listener to SIGTERM for closing
    if (signal(SIGTERM, termHandler) == SIG_ERR)
    {
        perror("processB - signal handler fro SIGTERM");
        exit(EXIT_FAILURE);
    }


    /*-------------------
        input arguments
    --------------------*/

    // check input arguments
    if (argc != 3)
    {
        printf("expected 2 input arguments, received %d\n", argc - 1);
        exit(EXIT_FAILURE);
    }

    // convert input argument to local variables
    const int pid_master = atoi(argv[1]);
    int isServer = 1;
    if (strncmp(argv[2],"c",1) == 0) {
        isServer = 0;
    }

    // data of the picture to save
    PICTURE *pSharedPicture;
    PICTURE localPicture;
    Point2D center;
    
    
    /*-------------------
        shared memory
    --------------------*/

    const int sharedMem_size = sizeof(*pSharedPicture);
    char sharedMem_name[32];
    setName(SHARED_MEM_NAME, isServer, sharedMem_name);
    int sharedMem_fd = shm_open(sharedMem_name, O_RDONLY, 0444);
    if (sharedMem_fd == -1) {
        perror("process B - failed shm_open");
        exit(EXIT_FAILURE);
    }

    ftruncate(sharedMem_fd, sharedMem_size);
    pSharedPicture = mmap(0, sharedMem_size,
        PROT_READ, MAP_SHARED, sharedMem_fd, 0);
    if(pSharedPicture == MAP_FAILED) {
        perror("process B - failed mapp");
        exit(EXIT_FAILURE);
    }


    /*-------------------
        semaphores
    --------------------*/
    char semaphore_write_name[32], semaphore_read_name[32];
    setName(SEMAPHORE_WRITE, isServer, semaphore_write_name);
    setName(SEMAPHORE_READ, isServer, semaphore_read_name);

    // open semaphore for writing, already created in process A
    pSemaphore_write = sem_open(semaphore_write_name, 0);
    if (pSemaphore_write == SEM_FAILED) {
        perror("process B - failed sem_open on pSemaphore_write");
        exit(EXIT_FAILURE);
    }

    // open semaphore for reading, already created in process A
    pSemaphore_read = sem_open(semaphore_read_name, 0);
    if (pSemaphore_read == SEM_FAILED) {
        perror("process B - failed sem_open on pSemaphore_read");
        exit(EXIT_FAILURE);
    }

    /*--------------------------------------------------------------------
				SEND SIGUSR2 WITH PROCESS B PID TO MASTER
                            INITITIALIZATION DONE
    --------------------------------------------------------------------*/
    union sigval value;
    value.sival_int = (int)getpid();

    if (sigqueue(pid_master, SIGUSR2, value) == -1)
    {
        perror("process B - sigqueue SIGUSR2 to master process");
        exit(EXIT_FAILURE);
    }

    /*--------------------------------------------------------------------
				                    GUI
    --------------------------------------------------------------------*/
    // Utility variable to avoid trigger resize event on launch
    int first_resize = TRUE;

    // Initialize UI
    init_console_ui();

    // adaptive scale factor, since konsole window size can change
    float scaleFactor_x = (float)getAreaColumns() / (float)PICTURE_WIDTH;
    float scaleFactor_y = (float)getAreaLines() / (float)PICTURE_HEIGHT;

    // update center position in GUI window
    if (updateCenterPosition(pSharedPicture, pSemaphore_read, pSemaphore_write,
            scaleFactor_x, scaleFactor_y) == -1)
    {   
        exit(EXIT_FAILURE);
    }

    // Infinite loop
    while (terminate == 0) {

        // Get input in non-blocking mode
        int cmd = getch();

        // If user resizes screen, re-draw UI...
        if(cmd == KEY_RESIZE) {
            if(first_resize) {
                first_resize = FALSE;
            }
            else {
                reset_console_ui();
            }
            // adapt picture scale factor on the window resizing
            scaleFactor_x = ((float)getAreaColumns() / (float)PICTURE_WIDTH);
            scaleFactor_y = ((float)getAreaLines() / (float)PICTURE_HEIGHT);
        }

        else {

            // update center position in GUI window
            if (updateCenterPosition(pSharedPicture, pSemaphore_read, pSemaphore_write,
                   scaleFactor_x, scaleFactor_y) == -1)
            {
                exit(EXIT_FAILURE);
            }
        }
    }

    /*--------------------------------------------------------------------
				            EXIT PROCESS B
    --------------------------------------------------------------------*/

    /*-------------------
        close GUI
    --------------------*/
    endwin();

    /*----------------------------
         semaphores clean up
    ----------------------------*/
    if (sem_close(pSemaphore_write) == -1) {
        perror("process B - failed sem_close pSemaphore_write");
        exit(EXIT_FAILURE);
    }
    if (sem_close(pSemaphore_read) == -1) {
        perror("process B - failed sem_close pSemaphore_read");
        exit(EXIT_FAILURE);
    }
    // unlinking is performed in master process

    /*----------------------------
        shared memory clean up
    ----------------------------*/
    if (munmap(pSharedPicture, sharedMem_size) == -1) {
        perror("process B - munmap failed");
        exit(EXIT_FAILURE);
    }
    // unlinking shared memory is done in master process

    // send negative value to master process
    // to specify process B closed properly
    value.sival_int = -1;
    if (sigqueue(pid_master, SIGUSR2, value) == -1)
    {
        perror("process A - failed sigqueue SIGUSR2 to master process");
        exit(EXIT_FAILURE);
    }

    return 0;
}

// update and refresh the position in the windows
int updateCenterPosition(PICTURE *pSharedPicture, sem_t *pSemaphore_read, sem_t *pSemaphore_write,
    float scaleFactor_x, float scaleFactor_y)
{
    PICTURE localPicture;
    Point2D center;

    // lock semaphore for reading
    if (sem_wait(pSemaphore_read) == -1) {
        if (errno != EINTR) {
            // does not fail if there is an incoming singal (i.e. SIGTERM)
            perror("process B - failed sem_wait on pSemaphore_read");
            return -1;
        }
    }

    // copy shared picture to local variable
    copySharedPicture(pSharedPicture, &localPicture);

    // unlock writing semaphore
    if (sem_post(pSemaphore_write) == -1) {
        perror("process B - failed sem_post on pSemaphore_write");
        return -1;
    }

    getCenter(&localPicture, &center);

    // scale center with respect to current window
    center.x = (int)roundf((float)center.x * scaleFactor_x);
    center.y = (int)roundf((float)center.y * scaleFactor_y);

    /*--------------------------------------------------------------------
                            UPDATE GUI POSITION
    --------------------------------------------------------------------*/

    mvaddch(center.y, center.x, '0');
    refresh();
}

// copy shared picture to local variable
void copySharedPicture(PICTURE* pSharedPicture, PICTURE *pLocalPicture) {
    for (int x = 0; x < PICTURE_WIDTH; x++)
    {
        for (int y = 0; y < PICTURE_HEIGHT; y++)
        {
            pLocalPicture->pixel[x][y] = pSharedPicture->pixel[x][y];
        }
    }
}

// estimate the center of a full circle in picture
void getCenter(PICTURE *picture, Point2D *center) {

    // Point2D center_red = {.x = 0, .y = 0};
    // Point2D center_green = {.x = 0, .y = 0};
    // Point2D center_blue = {.x = 0, .y = 0};
    CircleLimits limits_red =
        {.xmin = PICTURE_WIDTH+1, .xmax = -1, .ymin = PICTURE_HEIGHT+1, .ymax = -1};
    CircleLimits limits_green =
        {.xmin = PICTURE_WIDTH+1, .xmax = -1, .ymin = PICTURE_HEIGHT+1, .ymax = -1};
    CircleLimits limits_blue =
        {.xmin = PICTURE_WIDTH+1, .xmax = -1, .ymin = PICTURE_HEIGHT+1, .ymax = -1};
    unsigned char r=0,g=0,b=0;

    // find disk limits
    for (int x = 1; x < PICTURE_WIDTH; x++)
    {
        for (int y = 1; y < PICTURE_HEIGHT; y++)
        {
            // find red limits
            if (picture->pixel[x][y].red != 255) {
                r = 1;
                updateMinMax(x,y,&limits_red);
            }

            // find green limits
            if (picture->pixel[x][y].green != 255) {
                g = 1;
                updateMinMax(x,y,&limits_green);
            }

            // find blue limits
            if (picture->pixel[x][y].blue != 255) {
                b = 1;
                updateMinMax(x,y,&limits_blue);
            }
        }
    }

    // get cener of the disk
    center->x = (int)roundf(((r*limits_red.xmax + g*limits_green.xmax + b*limits_blue.xmax) + 
                    (r*limits_red.xmin + g*limits_green.xmin + b*limits_blue.xmin)) /
                        (2.0 * (r+g+b)));
    
    center->y = (int)roundf(((r*limits_red.ymax + g*limits_green.ymax + b*limits_blue.ymax) + 
                    (r*limits_red.ymin + g*limits_green.ymin + b*limits_blue.ymin)) /
                    (2.0 * (r+g+b)));
}

// updates the current limit of the circle
// given the current point withing the circle
void updateMinMax(int x,int y, CircleLimits *currLimits) {
                if (x < currLimits->xmin) {
                    currLimits->xmin = x;
                }
                if (x > currLimits->xmax) {currLimits->xmax = x;}
                if (y < currLimits->ymin) {currLimits->ymin = y;}
                if (y > currLimits->ymax) {currLimits->ymax = y;}
}
