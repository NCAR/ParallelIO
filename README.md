# ParallelIO

## Overview
This repository provides a robust implementation of **Parallel I/O (PIO)** using **NetCDF** and **ParallelNetCDF** backends. It is optimized for **high-performance computing (HPC)** and integrates with parallel file systems to efficiently handle large-scale scientific datasets.

## Features
- Parallel I/O support using **NetCDF** and **ParallelNetCDF**.
- Optimized **data rearrangement** for improved performance.
- **Asynchronous I/O** for reducing computational overhead.
- Compatible with **MPI-based distributed computing**.
- Extensive testing suite and continuous integration with **GitHub Actions**.

## Installation
### Prerequisites
Ensure you have the following dependencies installed:
- C and Fortran compilers (e.g., GCC, Intel, Clang)
- **MPI library** (MPICH or OpenMPI)
- **NetCDF** and **ParallelNetCDF** (built with MPI support)
- Either **CMake** or **Autotools**

### Build Instructions
#### Using CMake
```bash
mkdir build && cd build
cmake .. -DENABLE_FORTRAN=ON -DENABLE_NETCDF=ON
make -j$(nproc)
make install
```

#### Using Autotools
```bash
autoreconf -i
./configure --enable-fortran --enable-netcdf-integration
make -j$(nproc)
make install
```

## Running Tests
### Unit and Integration Tests
To run the test suite, use the following commands:
```bash
mpiexec -n 4 make check  # Run all unit tests
ctest -VV   # Run CMake-based tests (must be run on a parallel-capable node)
```

For debugging failed tests:
```bash
ctest --rerun-failed --output-on-failure  # Must be run on a parallel-capable node
```

## Usage
### Simple Example (C)
```c
#include <pio.h>
MPI_Init(NULL, NULL);
PIO_Init();
PIO_CreateFile("output.nc", PIO_WRITE, PIO_NETCDF);
PIO_CloseFile();
PIO_Finalize();
MPI_Finalize();
```

### Simple Example (Fortran)
```fortran
program test_pio
  use pio
  call MPI_Init()
  call PIO_Init()
  call PIO_CreateFile("output.nc", PIO_WRITE, PIO_NETCDF)
  call PIO_CloseFile()
  call PIO_Finalize()
  call MPI_Finalize()
end program test_pio
```

## Contributing
We welcome contributions! To contribute:
1. Fork the repository.
2. Create a feature branch.
3. Run tests before committing changes.
4. Submit a pull request with a clear description.

Refer to **doc/contributing_code.txt** for detailed guidelines.

## Documentation
- [NetCDF Homepage](https://www.unidata.ucar.edu/software/netcdf/)
- [ParallelNetCDF Homepage](https://parallel-netcdf.github.io/)
- [User Guide](doc/users_guide.txt)
- [API Documentation](doc/api.txt)
- [Testing Guidelines](doc/Testing.txt)

## License
This project is licensed under the **MIT License**. See the `LICENSE` file for details.


