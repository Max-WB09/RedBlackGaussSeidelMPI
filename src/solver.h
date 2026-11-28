#ifndef SOLVER_H
#define SOLVER_H

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int size;
    int max_iterations;
    double tolerance;
} Options;

static int parse_options(int argc, char **argv, Options *options) {
    *options = (Options){.size = 256, .max_iterations = 1000000,
                         .tolerance = 1e-4};
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc) return -1;
        char *end = NULL;
        errno = 0;
        if (strcmp(argv[i], "--size") == 0) {
            long value = strtol(argv[i + 1], &end, 10);
            if (errno || *end || value < 1 || value > 30000) return -1;
            options->size = (int)value;
        } else if (strcmp(argv[i], "--max-iter") == 0) {
            long value = strtol(argv[i + 1], &end, 10);
            if (errno || *end || value < 1 || value > 2147483647L) return -1;
            options->max_iterations = (int)value;
        } else if (strcmp(argv[i], "--tol") == 0) {
            double value = strtod(argv[i + 1], &end);
            if (errno || *end || !isfinite(value) || value <= 0.0) return -1;
            options->tolerance = value;
        } else {
            return -1;
        }
    }
    return 0;
}

static void print_usage(const char *program) {
    fprintf(stderr, "Usage: %s [--size N] [--tol T] [--max-iter K]\n", program);
}

static double *allocate_grid(int rows, int size, int first_global_row) {
    size_t width = (size_t)size + 2;
    if ((size_t)rows + 2 > SIZE_MAX / width / sizeof(double)) return NULL;
    double *grid = calloc(((size_t)rows + 2) * width, sizeof(double));
    if (!grid) return NULL;
    if (first_global_row == 1) {
        for (int col = 1; col <= size; ++col) grid[col] = 1.0;
    }
    return grid;
}

static double update_color(double *grid, int rows, int size,
                           int first_global_row, int color) {
    const size_t width = (size_t)size + 2;
    double max_change = 0.0;
    for (int row = 1; row <= rows; ++row) {
        int global_row = first_global_row + row - 1;
        int first_col = 1 + ((color - global_row - 1) & 1);
        for (int col = first_col; col <= size; col += 2) {
            size_t index = (size_t)row * width + (size_t)col;
            double previous = grid[index];
            double updated = 0.25 * (grid[index - width] + grid[index + width]
                                   + grid[index - 1] + grid[index + 1]);
            grid[index] = updated;
            double change = fabs(updated - previous);
            if (change > max_change) max_change = change;
        }
    }
    return max_change;
}

static double grid_checksum(const double *grid, int size) {
    double checksum = 0.0;
    size_t width = (size_t)size + 2;
    for (int row = 1; row <= size; ++row)
        for (int col = 1; col <= size; ++col)
            checksum += grid[(size_t)row * width + (size_t)col];
    return checksum;
}

static void print_header(void) {
    puts("mode,size,tolerance,processes,iterations,max_change,checksum,"
         "total_seconds,compute_seconds,verify_mean_seconds,"
         "verify_min_seconds,exchange_mean_seconds,serial_seconds");
}

#endif
