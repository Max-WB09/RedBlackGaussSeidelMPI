#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <time.h>
#include "solver.h"

static double now_seconds(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (double)time.tv_sec + 1e-9 * (double)time.tv_nsec;
}

int main(int argc, char **argv) {
    Options options;
    if (parse_options(argc, argv, &options)) {
        print_usage(argv[0]);
        return 1;
    }
    double start = now_seconds();
    double *grid = allocate_grid(options.size, options.size, 1);
    if (!grid) {
        fputs("Grid allocation failed\n", stderr);
        return 1;
    }
    double setup_end = now_seconds();
    double compute_seconds = 0.0;
    double max_change = 0.0;
    int iterations = 0;
    do {
        double phase_start = now_seconds();
        double red_change = update_color(grid, options.size, options.size, 1, 0);
        double black_change = update_color(grid, options.size, options.size, 1, 1);
        compute_seconds += now_seconds() - phase_start;
        max_change = fmax(red_change, black_change);
        ++iterations;
    } while (max_change >= options.tolerance &&
             iterations < options.max_iterations);

    double checksum = grid_checksum(grid, options.size);
    free(grid);
    double total_seconds = now_seconds() - start;
    double serial_seconds = total_seconds - compute_seconds;
    if (serial_seconds < setup_end - start) serial_seconds = setup_end - start;
    print_header();
    printf("sequential,%d,%.17g,1,%d,%.17g,%.17g,%.9f,%.9f,0,0,0,%.9f\n",
           options.size, options.tolerance, iterations, max_change, checksum,
           total_seconds, compute_seconds, serial_seconds);
    if (max_change >= options.tolerance) {
        fputs("Maximum iteration count reached before convergence\n", stderr);
        return 2;
    }
    return 0;
}
