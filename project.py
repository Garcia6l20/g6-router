#!/usr/bin/env python3

from cpppm import Project, main, Executable, Library

project = Project('g6-router')
project.requires = 'ctre/3.3.2'

router: Library = project.main_library()
router.include_dirs = 'include'
router.sources = 'include/g6/router.hpp'
router.link_libraries = 'ctre'
router.compile_options = '-std=c++20'

project.build_requires = 'boost/1.74.0', 'spdlog/1.8.1', 'fmt/7.1.2'
project.requires_options = {'boost:header_only': True}

# tests
router.tests_backend = 'catch2/2.13.3'

basic_test: Executable = project.executable('g6-router-basic-route-test')
basic_test.sources = 'tests/basic-route-test.cpp'
basic_test.link_libraries = 'fmt'

beast_example: Executable = project.executable('g6-http-router-example')
beast_example.sources = 'examples/http_router.cpp'
beast_example.link_libraries = router, 'boost', 'spdlog', 'fmt'

router.tests = basic_test, beast_example

if __name__ == '__main__':
    main()
