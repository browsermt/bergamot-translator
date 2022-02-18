#!/bin/bash

set -eo pipefail;

function run_coverage {
    SOURCE_DIR=$(python3 -c 'import bergamot as _; print(_.__path__[0])')
    coverage run -a --source ${SOURCE_DIR} -m bergamot ls
    coverage run -a --source ${SOURCE_DIR} -m bergamot download # Download all models
    coverage run -a --source ${SOURCE_DIR} -m bergamot translate -m en-de-tiny <<< "Hello World"
    coverage run -a --source ${SOURCE_DIR} -m bergamot translate -m en-de-tiny de-en-tiny <<< "Hello World"
    coverage run -a --source ${SOURCE_DIR} -m bergamot download -r opus -m eng-fin-tiny # Download specific model from opus
    coverage run -a --source ${SOURCE_DIR} -m bergamot translate --model eng-fin-tiny --repository opus <<< "Hello World"
    coverage run -a --source ${SOURCE_DIR} -m bergamot download -m eng-fin-tiny -r opus
    coverage run -a --source ${SOURCE_DIR} -m bergamot ls -r opus
    coverage run -a --source ${SOURCE_DIR} -m pytest --pyargs bergamot -rP
}

coverage erase

run_coverage # Run once.
run_coverage # Run again, to activate a few else paths.

coverage html

