#include <stdio.h>
#include <unistd.h>

/*
 * bomb - simple executable used for testing
 *      background processes
 */
int main(void)
{
    int cnt = 5;

    while (cnt--)
        sleep(1);

    printf("Boom!\n");
}
