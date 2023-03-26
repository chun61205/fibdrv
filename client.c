#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "bn_kernel.h"

#define FIB_DEV "/dev/fibonacci"

static inline long long elapsed(struct timespec *t1, struct timespec *t2)
{
    return (long long) (t2->tv_sec - t1->tv_sec) * 1e9 +
           (long long) (t2->tv_nsec - t1->tv_nsec);
}

int main()
{
    // long long sz;
    char buf[100];
    // char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */
    struct timespec t1, t2;

    int fd = open(FIB_DEV, O_RDWR);
    FILE *data = fopen("perf/time.txt", "w");

    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    if (!data) {
        perror("Failed to open data text");
        exit(2);
    }
    /*
    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 1);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%lld.\n",
               i, sz);
    }
    */

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        // sz = read(fd, buf, 1);
        read(fd, buf, 1);
        clock_gettime(CLOCK_MONOTONIC, &t2);
        ssize_t kt = write(fd, NULL, 0);
        long long utime = elapsed(&t1, &t2);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s. utime %lld, ktime %ld\n",
               i, buf, utime, kt);
        fprintf(data, "%d %lld %ld\n", i, utime, kt);
    }

    close(fd);
    fclose(data);
    return 0;
}