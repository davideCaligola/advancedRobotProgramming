#include <bmpfile.h>
#include <sys/signal.h>
#include <sys/mman.h>   // for shared memory
#include <sys/stat.h>   // for mode constants
#include <fcntl.h>      // for O_ constants
#include <semaphore.h>
#include <errno.h>
#include <string.h>
#include <regex.h>
#include <dirent.h>
#include "./../include/processA_utilities.h"
#include "./../include/helper.h"

/*--------------------------------------------------------------------
							SERVICE FUNCTIONS
--------------------------------------------------------------------*/
// create a picture of an ellipse reflecting the position with respect to the
// command window circle
bmpfile_t* createPicture(int height, int width, int center_x, int center_y, int radius_x, int radius_y);

// copy bmpfile to local shared variable
void bmpToPicture(bmpfile_t* bmp, PICTURE* picture);

// If the filename already exists, add an incremental number to it
int getFilename(char *outdir, char* fileName, char* fileExstension, char* outFileName);

// copy pictureBmp into shared memory
int sharePicture(bmpfile_t *pPictureBmp, PICTURE *pSharedPicture,
    sem_t *pSemaphore_write, sem_t *pSemaphore_read);

/*--------------------------------------------------------------------
                                MAIN
--------------------------------------------------------------------*/

int main(int argc, char *argv[])
{
    // check input arguments
    if (argc != 5)
    {
        printf("expected 4 input arguments, received %d\n", argc - 1);
        exit(EXIT_FAILURE);
    }

    // convert input argument to local variables
    const int pid_master = atoi(argv[1]);
    char *outdir = argv[2];
    char *filename_default = argv[3];
    char *fileExstension = argv[4];

    // data of the picture to save
    bmpfile_t *pPictureBmp;
    int pictureHeight = PICTURE_HEIGHT;
    int pictureWidth = PICTURE_WIDTH;
    float scaleFactor_x, scaleFactor_y;
    int circleRadius = 1;
    CIRCLE circleCenter;
    PICTURE *pSharedPicture;
    char filename[32];

    /*-------------------
        shared memory
    --------------------*/
    size_t sharedMem_size = sizeof(*pSharedPicture);
    int sharedMem_fd;
    sharedMem_fd = shm_open(SHARED_MEM_NAME, O_RDWR, 0666);
    if (sharedMem_fd == -1) {
        perror("process A - shm_open failed");
        exit(EXIT_FAILURE);
    }
    ftruncate(sharedMem_fd, sharedMem_size);
    pSharedPicture = mmap(0, sharedMem_size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED, sharedMem_fd, 0);
    if (pSharedPicture == MAP_FAILED) {
        perror("process A - mmap failed");
        exit(EXIT_FAILURE);
    }

    /*-------------------
        semaphores
    --------------------*/
    // open semaphore for writing
    sem_t *pSemaphore_write =sem_open(SEMAPHORE_WRITE, 0);
    if (pSemaphore_write == SEM_FAILED) {
        perror("process A - failed sem_open on pSemaphore_write");
        exit(EXIT_FAILURE);
    }

    // open semaphore for reading
    sem_t *pSemaphore_read = sem_open(SEMAPHORE_READ, 0);
    if (pSemaphore_read == SEM_FAILED) {
        perror("process A - failed sem_open on pSemaphore_read");
        exit(EXIT_FAILURE);
    }

    /*--------------------------------------------------------------------
				SEND SIGUSR1 WITH PROCESS a PID TO MASTER
                            INITITIALIZATION DONE
    --------------------------------------------------------------------*/
    union sigval value;
    value.sival_int = (int)getpid();

    if (sigqueue(pid_master, SIGUSR1, value) == -1)
    {
        perror("process A - sigqueue SIGUSR1 to master process");
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
    scaleFactor_x = (float)PICTURE_WIDTH / (float)getAreaColumns();
    scaleFactor_y = (float)PICTURE_HEIGHT / (float)getAreaLines();

    // get current circle center position
    circleCenter = getCircleCenter();

    // create bmp picture
    pPictureBmp = createPicture(pictureHeight, pictureWidth,
        (int)(circleCenter.x*scaleFactor_x), (int)(circleCenter.y*scaleFactor_y),
        (int)(circleRadius*scaleFactor_x), (int)(circleRadius*scaleFactor_y));

    if (sharePicture(pPictureBmp, pSharedPicture, pSemaphore_write, pSemaphore_read) == -1)
    {
        exit(EXIT_FAILURE);
    }

    int terminate = 0;
    // Infinite loop
    while (terminate == 0)
    {
        // Get input in non-blocking mode
        int cmd = getch();

        // If user resizes screen, re-draw UI...
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
            // adapt picture scale factor on the window resizing
            scaleFactor_x = (float)PICTURE_WIDTH / (float)getAreaColumns();
            scaleFactor_y = (float)PICTURE_HEIGHT / (float)getAreaLines();
        }

        // Else, if user presses print button...
        else if (cmd == KEY_MOUSE)
        {
            if (getmouse(&event) == OK)
            {
                /*----------------------
                      print button
                -----------------------*/
                if (check_button_pressed(print_btn, &event))
                {
                    // reset output file name
                    memset(filename, 0, sizeof(filename));

                    // get file name, in case there are already files with the
                    // same name
                    getFilename(outdir, filename_default, fileExstension, filename);

                    // get current circle center position
                    circleCenter = getCircleCenter();

                    // create bmp picture
                    pPictureBmp = createPicture(pictureHeight, pictureWidth,
                        (int)(circleCenter.x*scaleFactor_x), (int)(circleCenter.y*scaleFactor_y),
                        (int)(circleRadius*scaleFactor_x), (int)(circleRadius*scaleFactor_y));

                    if (sharePicture(pPictureBmp, pSharedPicture, pSemaphore_write, pSemaphore_read) == -1)
                    {
                        exit(EXIT_FAILURE);
                    }

                    // save picture with feedback message
                    if (!bmp_save(pPictureBmp, filename)) {
                        mvprintw(LINES - 1, 1, "some problem on saving the bmp file");
                    } else {
                        mvprintw(LINES - 1, 1, "bmp saved");
                    }
                    refresh();
                    sleep(1);
                    for(int j = 0; j < COLS - BTN_SIZE_X - 2; j++) {
                        mvaddch(LINES - 1, j, ' ');
                    }
                }

                /*----------------------
                       exit button
                -----------------------*/
                if (check_button_pressed(exit_btn, &event))
                {
                    terminate = 1;
                }
            }
        }

        // If input is an arrow key, move circle accordingly...
        else if (cmd == KEY_LEFT || cmd == KEY_RIGHT || cmd == KEY_UP || cmd == KEY_DOWN)
        {
            move_circle(cmd);
            draw_circle();

            // udapte picture data
            circleCenter = getCircleCenter();

            // create bmp picture
            pPictureBmp = createPicture(pictureHeight, pictureWidth,
                (int)(circleCenter.x*scaleFactor_x), (int)(circleCenter.y*scaleFactor_y),
                (int)(circleRadius*scaleFactor_x), (int)(circleRadius*scaleFactor_y));

            if (sharePicture(pPictureBmp, pSharedPicture, pSemaphore_write, pSemaphore_read) == -1)
            {
                exit(EXIT_FAILURE);
            }
        }
    }

    /*--------------------------------------------------------------------
				            EXIT PROCESS A
    --------------------------------------------------------------------*/
    /*-------------------
        close GUI
    --------------------*/
    endwin();

    /*----------------------------
        semaphores clean up
    ----------------------------*/
    if (sem_close(pSemaphore_write) == -1) {
        perror("process A - failed sem_close pSemaphore_write");
        exit(EXIT_FAILURE);
    }
    if (sem_close(pSemaphore_read) == -1) {
        perror("process A - failed sem_close pSemaphore_read");
        exit(EXIT_FAILURE);
    }
    // unlinking semaphore is done in master process

    /*----------------------------
        shared memory clean up
    ----------------------------*/
    if (munmap(pSharedPicture, sharedMem_size) == -1) {
        perror("process A - munmap failed");
        exit(EXIT_FAILURE);
    }
    // unlinking shared memory is done in master process

    // send negative value to master process
    // to specify process A closed properly
    printf("Out of while loop\n \
            Send finished signal to master process\n");
    fflush(stdout);
    value.sival_int = -1;
    if (sigqueue(pid_master, SIGUSR1, value) == -1)
    {
        perror("process A - sigqueue SIGUSR1 to master process");
        exit(EXIT_FAILURE);
    }

    return 0;
}

// create a picture of an ellipse reflecting the position with respect to the
// command window circle
bmpfile_t* createPicture(int height, int width, int center_x, int center_y, int radius_x, int radius_y)
{
    // Structure for bit image
    bmpfile_t *bmp;
    rgb_pixel_t pixel = {255,200,80,0}; // {blue, green, red, alpha}
    int depth = 4; // coloured image

    // create canvas
    bmp = bmp_create(width, height, depth);
    // draw a disc
    for (int x = -radius_x; x <= radius_x; x++)
    {
        for (int y = -radius_y; y <= radius_y; y++)
        {
            if (((float)(x*x) / (float)(radius_x*radius_x) +
                 (float)(y*y)/(float)(radius_y*radius_y)) < 1.0){
                // color the pixels within the radius distance
                bmp_set_pixel(bmp, center_x + x, center_y + y, pixel);
            }
        }
    }

    return bmp;
}

// copy bmpfile to local shared variable
void bmpToPicture(bmpfile_t* bmp, PICTURE* picture)
{
    for (int x = 0; x < PICTURE_WIDTH; x++)
    {
        for (int y = 0; y < PICTURE_HEIGHT; y++)
        {
            // get pixel information from bmp image
            rgb_pixel_t *gotPixel = bmp_get_pixel(bmp, x, y);

            // save locally pixel information
            picture->pixel[x][y].red = gotPixel->red;
            picture->pixel[x][y].green = gotPixel->green;
            picture->pixel[x][y].blue = gotPixel->blue;
            picture->pixel[x][y].alpha = gotPixel->alpha;
        }
    }
}

int sharePicture(bmpfile_t *pPictureBmp, PICTURE *pSharedPicture, sem_t *pSemaphore_write, sem_t *pSemaphore_read)
{
    // lock semaphore for writing
    if (sem_wait(pSemaphore_write) == -1) {
        if (errno != EINTR) {
            // does not fail if there is an incoming singal (i.e. SIGTERM)
            perror("process A - print button: failed sem_wait on pSemaphore_write");
            return -1;
        }
    }

    // copy information bmp picture to shared memory
    bmpToPicture(pPictureBmp, pSharedPicture);

    // unlock semaphore for reading
    if (sem_post(pSemaphore_read) == -1) {
        perror("process A - print button: failed sem_post on pSemaphore_read");
        return -1;
    }
}

// If the filename already exists, add an incremental number to it
int getFilename(char *outdir, char* fileName, char* fileExstension, char* outFileName) {
	// stream for reading out directory
	DIR* dir;
	// structure for reading the output directory
	struct dirent *entry;
	// structure to check if the output file already exists
	struct stat sb;
	// number of already numbered instance of the output file
	int counter = 0;

    // default output file name
    strcat(outFileName, outdir);
    strcat(outFileName, "/");
    strcat(outFileName, fileName);

	int ret = stat("./out/circle.bmp", &sb);
	if (ret  == -1)
	{
		// skip error due to output file does not exist yet
        // keep default file
        if (errno != ENOENT) {
			perror("looking for ./out/circle.bmp - stat");
			exit(EXIT_FAILURE);
		}
	} else { // file already exists
		dir = opendir("./out");
		if (dir == NULL) {
			perror("error opening directory ./out");
			exit(EXIT_FAILURE);
		}
		// read content of the directory checking how many numbered files
		// are already there
		errno = 0;
		regex_t regex_fileNumber;
		regmatch_t regMatch;
		// keeps on reading until there are not any more entries
		while ((entry = readdir(dir)) != NULL) {
			// check if the directory element has the default name
			if (strstr(entry->d_name, "circle") != NULL) {
				char msg[100];
				// by hypothesis, output file name is
				// circle.bmp or circle_XXX.bmp
				// regex for getting the number of the file name
				int retRegex = regcomp(&regex_fileNumber, "[0-9]+", REG_EXTENDED);
				if (retRegex) {
					printf("cannont compile regex\n");
					fflush(stdout);
					exit(EXIT_FAILURE);
				}
				// look for the file name number
				retRegex = regexec(&regex_fileNumber,entry->d_name,1,&regMatch,0);
				if (retRegex == 0) {
					int currentCopyNumber;
					int start = (int)regMatch.rm_so;
					int end = (int)regMatch.rm_eo;
					char numberOfCopy[5]; // maximum number of copies is 999
					memset(numberOfCopy, 0, sizeof(numberOfCopy));
					// extract the number of the copy from the file number
					strncpy(numberOfCopy, &(entry->d_name[start]), end - start);
					currentCopyNumber = atoi(numberOfCopy);
					// store the current file number if it is the maximum value
					if (currentCopyNumber > counter)
						counter = currentCopyNumber;
				}
				// skip the case where there is no match
				else if (retRegex != REG_NOMATCH) {
					regerror(retRegex, &regex_fileNumber, msg, sizeof(msg));
					perror(msg);
					exit(EXIT_FAILURE);
				}
			}
		}
		// free memory from regex
		regfree(&regex_fileNumber);

		if (errno && !entry) {
			perror("readdir");
		}
		if (closedir(dir) == -1) {
			perror("error while closing directory ./out");
			exit(EXIT_FAILURE);
		}

        // add file counter
        // convert the maximum counter value of the already present output files
        char counterS[4];
        sprintf(counterS,"%d", counter+1);
        // create new file name with the fle number
        strcat(outFileName, "_");
        strcat(outFileName, counterS);
	}

	strcat(outFileName, fileExstension);

	return 0;
}
