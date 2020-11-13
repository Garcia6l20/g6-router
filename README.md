# General purpose CPP router

This project provides a c++20 router implementation.

It uses [compile-time-regular-expression](https://github.com/hanickadot/compile-time-regular-expressions) for route pattern matching.

## Install

This is a header-only library so, just grab a copy of it.

## Devel

It uses [cpppm](https://github.com/Garcia6l20/cpppm) internally (for testing).

Build example:

1. Get cpppm
```bash
pip3 install --user --upgrade cpppm
```

2. Build & Test
```bash
./project.py test g6-router
```
