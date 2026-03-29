This repository just contains a collection of small experiments with C++26's static reflection.

The makefile is configured to build with the [latest GCC-16 snapshot](https://gcc.gnu.org/pub/gcc/snapshots/LATEST-16/). GCC 16 is building in support for C++26 and its static reflection features, but is still in development.

If for some reason you want to build these "experiments" on your own, you'll need to compile GCC-16, and then substitute `LOCAL_GCC16_DIRECTORY` in the makefile with your local directory.

There's nothing useful here however, it's just playing around with C++26.
