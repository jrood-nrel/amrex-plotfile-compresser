# amrex-plotfile-compresser

Convert native [AMReX](https://amrex-codes.github.io/amrex/) plotfiles to
HDF5 format with optional ZFP (or zlib) compression. The conversion can be
parallelised across many MPI ranks, making it practical to post-process large
simulation datasets.

## Features

- Reads any number of native AMReX plotfiles (multi-level, arbitrary components)
- Writes compressed HDF5 plotfiles readable by VisIt / ParaView (Chombo reader)
- Configurable compression: ZFP accuracy/rate/precision/reversible or zlib
- MPI-parallel I/O via AMReX's collective HDF5 writer
- Configurable via a ParmParse inputs file or command-line key=value pairs

## Prerequisites

| Dependency | Notes |
|---|---|
| CMake ≥ 3.20 | |
| C++17 compiler | |
| MPI | e.g. OpenMPI or MPICH |
| Parallel HDF5 | **must** be the parallel variant (`--enable-parallel`) |
| H5Z-ZFP ≥ 1.0.1 | optional, needed for ZFP compression; AMReX uses `find_package(H5Z_ZFP 1.0.1 CONFIG)` |

> **Important:** AMReX's `AMReX_HDF5_ZFP=ON` option requires a **parallel** HDF5
> library (built with MPI support). Serial HDF5 will cause a CMake error.

### Environment variables

Set these before running CMake so that AMReX's `find_package` calls find the
right libraries:

```bash
# Path to the HDF5 installation (parallel build)
export HDF5_ROOT=/path/to/parallel-hdf5

# Path to the H5Z-ZFP cmake config directory (contains H5Z_ZFPConfig.cmake)
export H5Z_ZFP_DIR=/path/to/h5z-zfp/lib/cmake/h5z_zfp
```

## Building

```bash
# 1. Clone and initialise submodule
git clone https://github.com/jrood-nrel/amrex-plotfile-compresser.git
cd amrex-plotfile-compresser
git submodule update --init --recursive

# 2. Configure
cmake -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DAMReX_SPACEDIM=3           # default; set to 2 for 2-D plotfiles

# Override if needed (these are all ON/OFF by default as set in CMakeLists.txt):
#   -DAMReX_MPI=ON
#   -DAMReX_HDF5=ON
#   -DAMReX_HDF5_ZFP=ON

# 3. Build
cmake --build build -j$(nproc)
```

The resulting executable is `build/amrex-plotfile-compresser`.

## Running

### Serial

```bash
./build/amrex-plotfile-compresser inputs/example.inp
```

### Parallel with MPI

```bash
mpirun -np 8 ./build/amrex-plotfile-compresser inputs/example.inp
```

### Inline options (no inputs file)

```bash
mpirun -np 4 ./build/amrex-plotfile-compresser \
    plotfiles=plt00000 plt01000 \
    hdf5_compression=ZFP_ACCURACY@0.001
```

## Options

All options can be set in an inputs file or on the command line as
`key=value` pairs.

| Option | Default | Description |
|---|---|---|
| `plotfiles` | *(required)* | Space-separated list of native AMReX plotfile directories to convert |
| `hdf5_compression` | `ZFP_ACCURACY@0.001` | HDF5 compression descriptor (see table below) |
| `output_prefix` | *(none)* | If set, output files are named `<prefix>_<leaf>.h5`; otherwise `<input>.h5` |

### Compression descriptors

| Descriptor | Notes |
|---|---|
| `None@0` | No compression – plain HDF5 |
| `ZLIB@<level>` | zlib / deflate; `<level>` = 1–9 (e.g. `ZLIB@6`) |
| `ZFP_RATE@<rate>` | ZFP fixed-rate; `<rate>` = bits per floating-point value (e.g. `ZFP_RATE@4`) |
| `ZFP_PRECISION@<prec>` | ZFP fixed-precision; `<prec>` = uncompressed bits (e.g. `ZFP_PRECISION@16`) |
| `ZFP_ACCURACY@<acc>` | ZFP fixed-accuracy (lossy, default); `<acc>` = absolute error bound (e.g. `ZFP_ACCURACY@0.001`) |
| `ZFP_REVERSIBLE@reversible` | ZFP lossless reversible mode |

## Sample inputs file

See [`inputs/example.inp`](inputs/example.inp) for a fully commented example.

## Output

For each input plotfile `plt00000` the converter produces `plt00000.h5` (or
`<prefix>_plt00000.h5` if `output_prefix` is set).  The HDF5 files follow the
AMReX / Chombo layout and can be opened in VisIt or ParaView with the Chombo
reader.

## License

MIT — see [LICENSE](LICENSE).
