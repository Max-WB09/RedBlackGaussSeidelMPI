#!/usr/bin/env python3
"""Compare sequential and MPI results across uneven row partitions."""

import csv
import io
import math
import os
import subprocess


def run(command):
    environment = os.environ.copy()
    environment.setdefault("OMPI_ALLOW_RUN_AS_ROOT", "1")
    environment.setdefault("OMPI_ALLOW_RUN_AS_ROOT_CONFIRM", "1")
    output = subprocess.check_output(command, text=True, env=environment)
    return next(csv.DictReader(io.StringIO(output)))


def main():
    for size in (5, 8, 11, 17):
        options = ["--size", str(size), "--tol", "1e-7"]
        expected = run(["build/sequential", *options])
        for processes in (1, 2, 3, 4):
            actual = run(["mpirun", "-np", str(processes),
                          "build/parallel", *options])
            for field in ("iterations", "max_change", "checksum"):
                if field == "iterations":
                    assert actual[field] == expected[field], (
                        size, processes, field, actual[field], expected[field]
                    )
                else:
                    assert math.isclose(float(actual[field]),
                                        float(expected[field]),
                                        rel_tol=1e-14, abs_tol=1e-14), (
                        size, processes, field, actual[field], expected[field]
                    )
            print(f"PASS size={size} processes={processes} "
                  f"iterations={actual['iterations']}")


if __name__ == "__main__":
    main()
