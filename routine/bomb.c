#include <stdio.h>
#include <unistd.h>

/*
 * bomb - simple executable used for testing
 *      background processes
 */
int main(int argc, char **argv)
{
    int clck = 10;

    int output = argc > 1;

    while (--clck > 0) {
        if (output)
            printf("%d\n", clck);
        sleep(1);
    }

    printf("Boom!\n");
}
