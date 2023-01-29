# ARP-Assignment2
Base repository for the **second ARP assignment**.
The project provides you with a base infrastructure for the implementation of the simulated vision system through shared memory, according to the requirements specified in the PDF file of the assignment.

The two processes involved in the simulation of the vision system, namely **processA** and **processB**, are implemented as simple *ncurses windows*. The development of the inter-process communication pipeline, that is the shared memory, is left to you.

As for the first assignment, you also find a **master** process already prepared for you, responsible of spawning the entire simulation.

Additionally, I have prepared a simple program called **circle.c**, which shows you the basic functionalities of the *libbitmap* library. Please, note that the **circle.c** process must not appear in your final project. It is simply meant to be a guide for you on how to use the bitmap library, therefore you will need to properly use portions of that code in **processA** and **processB** in order to develop your solution.

## *libbitmap* installation and usage
To work with the bitmap library, you need to follow these steps:
1. Download the source code from [this GitHub repo](https://github.com/draekko/libbitmap.git) in your file system.
2. Navigate to the root directory of the downloaded repo and run the configuration through command ```./configure```. Configuration might take a while.  While running, it prints some messages telling which features it is checking for.
3. Type ```make``` to compile the package.
4. Run ```make install``` to install the programs and any data files and documentation.
5. Upon completing the installation, check that the files have been properly installed by navigating to ```/usr/local/lib```, where you should find the ```libbmp.so``` shared library ready for use.
6. In order to properly compile programs which use the *libbitmap* library, you first need to notify the **linker** about the location of the shared library. To do that, you can simply add the following line at the end of your ```.bashrc``` file:      
```export LD_LIBRARY_PATH="/usr/local/lib:$LD_LIBRARY_PATH"```
### Using *libbitmap* in your code
Now that you have properly installed the library in your system, it's time to use it in your programs:
1. Include the library in your programs via ```#include <bmpfile.h>```. If you want to check the content of ```bmpfile.h``` to glimpse the functionalities of the library, navigate to ```/usr/local/include```, where the header file should be located.
2. Compile programs which use the *libbitmap* library by linking the shared library with the ```-lbmp``` command.     
Example for compiling **circle.c**: ```gcc src/circle.c -lbmp -lm -o bin/circle``` 

## Compiling and running **processA** and **processB**
The two processes are implemented as UIs through *ncurses* library, therefore you need to compile their source files by linking the shared library via ```-lncurses```. As for the first assignment, exploit the resize event of the windows to get out of situations in which the graphical elements do not properly spawn.

## Executing **circle.c**
This is a simple example of a program which uses the *libbitmap* library. It generates a 100x100 colored `.bmp` file with user-defined name, depicting a blue circle of given radius. When you execute it, pass the two arguments (file name and radius value) along. Execution example: ```./bin/circle out/test.bmp 20```.

