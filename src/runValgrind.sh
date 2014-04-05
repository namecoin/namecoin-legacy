#!/bin/bash

valgrind --leak-check=full ./namecoind 2>&1 | tee valgrind.log
