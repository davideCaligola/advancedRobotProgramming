#ifndef __helper_h__
#define __helper_h__

#include <string.h>

#define SHARED_MEM_NAME "/sharedPicture"
#define SEMAPHORE_WRITE "/semaphore_write"
#define SEMAPHORE_READ "/semaphore_read"
#define PICTURE_HEIGHT 600
#define PICTURE_WIDTH 1600

typedef struct {
    unsigned char red, green, blue, alpha;
} RGBA;

typedef struct {
    RGBA pixel[PICTURE_WIDTH][PICTURE_HEIGHT];
} PICTURE;


/* returns preName_client if mode = 0
   or preName_server otherwise.
*/
void setName(char *preName, int mode, char *name){
    strncpy(name, preName, strlen(preName)+1);
    if (mode == 0) {
        strncat(name, "_client", strlen("_client")+1);
    } else {
        strncat(name, "_server", strlen("_server")+1);
    }
}


#endif