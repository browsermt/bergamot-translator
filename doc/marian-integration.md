# Building marian code for bergamot

This document summarizes the minimal build instructions develop for the
marian machine translation toolkit powering bergamot-translator.

## Build Instructions

Marian CPU version requires Intel MKL or OpenBLAS. Both are free, but MKL is not open-sourced. Intel MKL is strongly recommended as it is faster. On Ubuntu 16.04 and newer it can be installed from the APT repositories.

```bash
wget -qO- 'https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS-2019.PUB' | sudo apt-key add -
sudo sh -c 'echo deb https://apt.repos.intel.com/mkl all main > /etc/apt/sources.list.d/intel-mkl.list'
sudo apt-get update
sudo apt-get install intel-mkl-64bit-2020.0-088
```
On MacOS, apple accelerate framework will be used instead of MKL/OpenBLAS.

```
$ git clone https://github.com/browsermt/bergamot-translator
$ cd bergamot-translator
$ mkdir build
$ cd build
$ cmake .. -DUSE_WASM_COMPATIBLE_SOURCE=off -DCMAKE_BUILD_TYPE=Release
$ make -j
```


The build will generate the library that can be linked to any project. All the
public header files are specified in `src` folder.

## Command line apps

The following executables are created by the build:

1. `app/service-cli`: Extends marian to capability to work with string_views.
   `service-cli` exists to check if the underlying code, without the
   integration works or not.
2. `app/bergamot-translator-app`: App which integreates service-cli's
   functionality into the translator agnostic API specified as part of the
   project. Integration failures are detected if same arguments work with
   `service-cli` and does not with `bergamot-translator-app`.
3. `app/marian-decoder-new`: Helper executable to conveniently benchmark new
   implementation with the optimized upstream marian-decoder.

The models required to run the command-line are available at
[data.statmt.org/bergamot/models/](http://data.statmt.org/bergamot/models/).
The following example uses an English to German tiny11 student model, available
at:

* [data.statmt.org/bergamot/models/deen/ende.student.tiny11.tar.gz](http://data.statmt.org/bergamot/models/deen/ende.student.tiny11.tar.gz)

<details>
<summary> Example run of commandline: Click to expand </summary>
<p>

```bash
MODEL_DIR=... # path to where the model-files are.
ARGS=(
    -m $MODEL_DIR/model.intgemm.alphas.bin # Path to model file.
    --vocabs 
        $MODEL_DIR/vocab.deen.spm # source-vocabulary
        $MODEL_DIR/vocab.deen.spm # target-vocabulary

    # The following increases speed through one-best-decoding, shortlist and quantization.
    --beam-size 1 --skip-cost --shortlist $MODEL_DIR/lex.s2t.bin false --int8shiftAlphaAll 

    # Number of CPU threads (workers to launch). Parallelizes over cores and improves speed.
    # A value of 0 allows a path with no worker thread-launches and a single-thread.
    --cpu-threads 4

    # Maximum size of a sentence allowed. If a sentence is above this length,
    # it's broken into pieces of less than or equal to this size.
    --max-length-break 1024  

    # Maximum number of tokens that can be fit in a batch. The optimal value 
    # for the parameter is dependant on hardware and can be obtained by running
    # with variations and benchmarking.
    --mini-batch-words 1024 

    # Three modes are supported
    #   - sentence: One sentence per line
    #   - paragraph: One paragraph per line.
    #   - wrapped_text: Paragraphs are separated by empty line.
    --ssplit-mode paragraph 
)

./app/service-cli "${ARGS[@]}" < path-to-input-file
./app/bergamot-translator-app "${ARGS[@]}" < path-to-input-file

```
</p>

</summary>
</details>

## Coding Style

This repository contains C++ and JS source-files, of which C++ should adhere to
the clang-format based style guidelines. You may configure your development
environment to use the `.clang-format` and `.clang-format-ignore` files
provided in the root folder of this repository with your preferred choice of
editor/tooling.

One simple and recommended method to get your code to adhere to this style is
to issue the following command in the source-root of this repository, which is
used to also check for the coding style in the CI.

```bash
python3 run-clang-format.py -i --style file -r src wasm
```

