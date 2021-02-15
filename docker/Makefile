# -*- mode: makefile-gmake; indent-tabs-mode: true; tab-width: 4 -*-
SHELL   		= bash
PWD     		= $(shell pwd)
WASM_IMAGE	    = local/bergamot-translator-build-wasm

all: wasm-image compile-wasm

# Build the Docker image for WASM builds
wasm-image:
	docker build -t local/bergamot-translator-build-wasm ./wasm/

# Commands for compilation:
cmake_cmd  = cmake

wasm_cmake_cmd = ${cmake_cmd}
wasm_cmake_cmd += -DCOMPILE_WASM=on
wasm_cmake_cmd += -DProtobuf_INCLUDE_DIR=/usr/opt/protobuf-wasm-lib/dist/include
wasm_cmake_cmd += -DProtobuf_LIBRARY=/usr/opt/protobuf-wasm-lib/dist/lib/libprotobuf.a
wasm_cmake_cmd += -DPACKAGE_DIR=/repo/models

make_cmd  = make
#make_cmd += VERBOSE=1

# ... and running things on Docker
docker_mounts  = ${PWD}/..:/repo
docker_mounts += ${HOME}/.ccache:/.ccache
run_on_docker  = docker run --rm
run_on_docker += $(addprefix -v, ${docker_mounts})
run_on_docker += ${INTERACTIVE_DOCKER_SESSION}

${HOME}/.ccache:
	mkdir -p $@

# Remove the bergamot-translator WASM build dir, forcing a clean compilation attempt
clean-wasm: BUILD_DIR = /repo/build-wasm-docker
clean-wasm: ${HOME}/.ccache
	${run_on_docker} ${WASM_IMAGE} bash -c '(rm -rf ${BUILD_DIR} || true)'

# Compile bergamot-translator to WASM
compile-wasm: BUILD_DIR = /repo/build-wasm-docker
compile-wasm: ${HOME}/.ccache
	${run_on_docker} ${WASM_IMAGE} bash -c 'mkdir -p ${BUILD_DIR} && \
cd ${BUILD_DIR} && \
(emcmake ${wasm_cmake_cmd} .. && \
(emmake ${make_cmd}) || \
rm CMakeCache.txt)'

# Start interactive shells for development / debugging purposes
native-shell: INTERACTIVE_DOCKER_SESSION = -it
native-shell:
	${run_on_docker} ${NATIVE_IMAGE} bash

wasm-shell: INTERACTIVE_DOCKER_SESSION = -it
wasm-shell:
	${run_on_docker} ${WASM_IMAGE} bash
