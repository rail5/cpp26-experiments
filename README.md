This repository just contains a collection of small experiments with C++26's new features.

Experiments are `.cpp` files with name prefixes by category (e.g., `reflection-` for experiments with C++26's static reflection features). The makefile is configured to build all of these experiments, and the resulting binaries are placed in `bin/`.

The makefile is configured to build with the [latest GCC-16 snapshot](https://gcc.gnu.org/pub/gcc/snapshots/LATEST-16/). GCC 16 is building in support for C++26 and its static reflection features, but is still in development.

If for some reason you want to build these "experiments" on your own, you'll need to compile GCC-16, and then substitute `LOCAL_GCC16_DIRECTORY` in the makefile with your local directory.

There's nothing useful here however, it's just playing around with C++26.

---

Footnote: [C++26 is done](https://mpusz.github.io/mp-units/HEAD/blog/2026/03/28/report-from-the-croydon-2026-iso-c-committee-meeting/#c26-is-done). The standard has been officially shipped. The next step is to implement the features in compilers, and then we can start using them in production code. In the meantime, we can experiment with them in GCC-16's snapshot builds.
