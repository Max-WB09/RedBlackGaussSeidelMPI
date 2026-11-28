#include <stdint.h>
#include <mpi.h>
#include "solver.h"

static void exchange_halos(double *grid, int rows, int size, int rank,
                           int processes) {
    int up = rank == 0 ? MPI_PROC_NULL : rank - 1;
    int down = rank == processes - 1 ? MPI_PROC_NULL : rank + 1;
    int width = size + 2;
    MPI_Sendrecv(grid + width, width, MPI_DOUBLE, up, 10,
                 grid + (size_t)(rows + 1) * width, width, MPI_DOUBLE,
                 down, 10, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Sendrecv(grid + (size_t)rows * width, width, MPI_DOUBLE, down, 11,
                 grid, width, MPI_DOUBLE, up, 11, MPI_COMM_WORLD,
                 MPI_STATUS_IGNORE);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, processes;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &processes);
    Options options;
    int valid = parse_options(argc, argv, &options) == 0;
    if (!valid || options.size < processes) {
        if (rank == 0) {
            print_usage(argv[0]);
            fputs("Grid size must be at least the process count\n", stderr);
        }
        MPI_Finalize();
        return 1;
    }

    double start = MPI_Wtime();
    int base = options.size / processes;
    int extra = options.size % processes;
    int rows = base + (rank < extra);
    int first_row = rank * base + (rank < extra ? rank : extra) + 1;
    double *grid = allocate_grid(rows, options.size, first_row);
    int allocated = grid != NULL;
    int all_allocated;
    MPI_Allreduce(&allocated, &all_allocated, 1, MPI_INT, MPI_MIN,
                  MPI_COMM_WORLD);
    if (!all_allocated) {
        if (rank == 0) fputs("Grid allocation failed\n", stderr);
        free(grid);
        MPI_Finalize();
        return 1;
    }

    double compute_seconds = 0.0, verify_seconds = 0.0;
    double exchange_seconds = 0.0, max_change = 0.0;
    int iterations = 0;
    do {
        double phase_start = MPI_Wtime();
        double red_change = update_color(grid, rows, options.size, first_row, 0);
        compute_seconds += MPI_Wtime() - phase_start;

        phase_start = MPI_Wtime();
        exchange_halos(grid, rows, options.size, rank, processes);
        exchange_seconds += MPI_Wtime() - phase_start;

        phase_start = MPI_Wtime();
        double black_change = update_color(grid, rows, options.size, first_row, 1);
        compute_seconds += MPI_Wtime() - phase_start;

        double local_change = fmax(red_change, black_change);
        phase_start = MPI_Wtime();
        MPI_Allreduce(&local_change, &max_change, 1, MPI_DOUBLE, MPI_MAX,
                      MPI_COMM_WORLD);
        verify_seconds += MPI_Wtime() - phase_start;
        ++iterations;
        if (max_change < options.tolerance ||
            iterations == options.max_iterations) break;

        phase_start = MPI_Wtime();
        exchange_halos(grid, rows, options.size, rank, processes);
        exchange_seconds += MPI_Wtime() - phase_start;
    } while (1);

    /* Assemble rows in global order so the checksum uses sequential summation. */
    int width = options.size + 2;
    double *full = NULL;
    int *counts = NULL, *offsets = NULL;
    if (rank == 0) {
        full = calloc(((size_t)options.size + 2) * width, sizeof(double));
        counts = malloc((size_t)processes * sizeof(int));
        offsets = malloc((size_t)processes * sizeof(int));
    }
    int gather_ready = rank != 0 || (full && counts && offsets);
    MPI_Bcast(&gather_ready, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (!gather_ready) {
        if (rank == 0) fputs("Summary allocation failed\n", stderr);
        free(grid); free(full); free(counts); free(offsets);
        MPI_Finalize();
        return 1;
    }
    if (rank == 0) {
        int displacement = width;
        for (int process = 0; process < processes; ++process) {
            counts[process] = (base + (process < extra)) * width;
            offsets[process] = displacement;
            displacement += counts[process];
        }
    }
    MPI_Gatherv(grid + width, rows * width, MPI_DOUBLE, full, counts,
                offsets, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    double checksum = rank == 0 ? grid_checksum(full, options.size) : 0.0;

    double phase_times[3] = {compute_seconds, verify_seconds, exchange_seconds};
    double mean_times[3], min_verify;
    MPI_Reduce(phase_times, mean_times, 3, MPI_DOUBLE, MPI_SUM, 0,
               MPI_COMM_WORLD);
    MPI_Reduce(&verify_seconds, &min_verify, 1, MPI_DOUBLE, MPI_MIN, 0,
               MPI_COMM_WORLD);
    double elapsed = MPI_Wtime() - start;
    double total_seconds;
    MPI_Reduce(&elapsed, &total_seconds, 1, MPI_DOUBLE, MPI_MAX, 0,
               MPI_COMM_WORLD);
    if (rank == 0) {
        print_header();
        printf("parallel,%d,%.17g,%d,%d,%.17g,%.17g,%.9f,%.9f,%.9f,%.9f,%.9f,0\n",
               options.size, options.tolerance, processes, iterations,
               max_change, checksum, total_seconds,
               mean_times[0] / processes, mean_times[1] / processes,
               min_verify, mean_times[2] / processes);
        if (max_change >= options.tolerance)
            fputs("Maximum iteration count reached before convergence\n", stderr);
    }
    free(grid); free(full); free(counts); free(offsets);
    MPI_Finalize();
    return max_change < options.tolerance ? 0 : 2;
}
