#!/bin/bash

COMPILER=$1

$COMPILER -march=native -E -v - < /dev/null 2>&1 | grep cc1 \
    | sha256sum | cut -c1-8

