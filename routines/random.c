#include <stdio.h>
#include <stdlib.h>
#include <time.h>
int main() {
    struct timespec start;
    clock_gettime(CLOCK_MONOTONIC_RAW, &start);
    srand(start.tv_sec + start.tv_nsec);
    double rand1 = (double)rand();
    double rand2 = (double)rand();
    
    if (rand1 > rand2)
    {
        printf("%f", (rand2/rand1));
    }
    else
    {
        printf("%f", (rand1/rand2));
    }
    
    return 0;
}
