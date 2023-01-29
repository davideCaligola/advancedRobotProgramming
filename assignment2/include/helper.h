#ifndef __helper_h__
#define __helper_h__

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

#endif