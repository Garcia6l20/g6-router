#!/usr/bin/env python3

from cpppm import Project, main

project = Project('g6-router')
project.requires = 'ctre/3.3.2'

router = project.main_library()
router.include_dirs = 'include'
router.sources = 'include/g6/router.hpp'
router.link_libraries = 'ctre'
router.compile_options = '-std=c++20'

# tests
router.tests_backend = 'catch2/2.13.3'

router.tests = 'tests/basic-route-test.cpp'

if __name__ == '__main__':
    main()
