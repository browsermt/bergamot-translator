# Bergamot C++ Library

This document contains instructions to develop for modifications on top of the
marian machine translation toolkit powering bergamot-translator. The library is
optimized towards fast and efficient translation of a given input.

## Build Instructions

Note: You are strongly advised to refer to the continuous integration on this
repository, which builds bergamot-translator and associated applications from
scratch. Examples to run these command line-applications are available in the
[bergamot-translator-tests](https://github.com/browsermt/bergamot-translator-tests)
repository. Builds take about 30 mins on a consumer grade machine, so using a
tool like ccache is highly recommended.

### Dependencies 

Marian CPU version requires Intel MKL or OpenBLAS. Both are free, but MKL is
not open-sourced. Intel MKL is strongly recommended as it is faster. On Ubuntu
16.04 and newer it can be installed from the APT repositories.

```bash
wget -qO- 'https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS-2019.PUB' | sudo apt-key add -
sudo sh -c 'echo deb https://apt.repos.intel.com/mkl all main > /etc/apt/sources.list.d/intel-mkl.list'
sudo apt-get update
sudo apt-get install intel-mkl-64bit-2020.0-088
```
On MacOS, apple accelerate framework will be used instead of MKL/OpenBLAS.


### Building bergamot-translator

Web Assembly (WASM) reduces building to only using a subset of functionalities
of marian, the translation library powering bergamot-translator. When
developing bergamot-translator it is important that the sources added be
compatible with marian.  Therefore, it is required to set
`-DUSE_WASM_COMPATIBLE_SOURCE=on`.

```
$ git clone https://github.com/browsermt/bergamot-translator
$ cd bergamot-translator
$ mkdir build
$ cd build
$ cmake .. -DUSE_WASM_COMPATIBLE_SOURCE=off -DCMAKE_BUILD_TYPE=Release
$ make -j2 
```

The build will generate the library that can be linked to any project. All the
public header files are specified in `src` folder.

## Command line apps

Bergamot-translator is intended to be used as a library. However, we provide a
command-line application which is capable of translating text provided on
standard-input. During development this application is used to perform
regression-tests.

There are effectively multiple CLIs subclassed from a unified interface all
provided in `app/cli.h`. These are packed into a single executable named
`bergamot` by means of a `--bergamot-mode BERGAMOT_MODE` switch. 

The following modes are available:

* `--bergamot-mode native` 
* `--bergamot-mode wasm`    
* `--bergamot-mode decoder` 

Find documentation on these modes with the API documentation for apps [here](./api/namespace_marian__bergamot__app.html#functions).

## Example command line run

The models required to run the command-line are available at
[data.statmt.org/bergamot/models/](http://data.statmt.org/bergamot/models/).
The following example uses an English to German tiny11 student model, available
at:

* [data.statmt.org/bergamot/models/deen/ende.student.tiny11.tar.gz](http://data.statmt.org/bergamot/models/deen/ende.student.tiny11.tar.gz)

```bash
MODEL_DIR=... # path to where the model-files are.
BERGAMOT_MODE='native'
ARGS=(
    --bergamot-mode $BERGAMOT_MODE
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

./app/bergamot "${ARGS[@]}" < path-to-input-file

```


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
