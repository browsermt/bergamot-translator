# Bergamot Translator

[![CircleCI badge](https://img.shields.io/circleci/project/github/browsermt/bergamot-translator/main.svg?label=CircleCI)](https://circleci.com/gh/browsermt/bergamot-translator/)

Bergamot translator provides a unified API for ([Marian NMT](https://marian-nmt.github.io/) framework based) neural machine translation functionality in accordance with the [Bergamot](https://browser.mt/) project that focuses on improving client-side machine translation in a web browser.

## Build Instructions

### Build Natively
Create a folder where you want to build all the artifacts (`build-native` in this case) and compile

```bash
mkdir build-native
cd build-native
cmake ../
make -j2
```

### Build WASM
#### Prerequisite

Building on wasm requires Emscripten toolchain. It can be downloaded and installed using following instructions:

* Get the latest sdk: `git clone https://github.com/emscripten-core/emsdk.git`
* Enter the cloned directory: `cd emsdk`
* Install the sdk: `./emsdk install 3.1.8`
* Activate the sdk: `./emsdk activate 3.1.8`
* Activate path variables: `source ./emsdk_env.sh`

#### <a name="Compile"></a> Compile

To build a version that translates with higher speeds on Firefox Nightly browser, follow these instructions:

   1. Create a folder where you want to build all the artifacts (`build-wasm` in this case) and compile
       ```bash
       mkdir build-wasm
       cd build-wasm
       emcmake cmake -DCOMPILE_WASM=on ../
       emmake make -j2
       ```

       The wasm artifacts (.js and .wasm files) will be available in the build directory ("build-wasm" in this case).

   2. Patch generated artifacts to import GEMM library from a separate wasm module
       ```bash
       bash ../wasm/patch-artifacts-import-gemm-module.sh
       ```

To build a version that runs on all browsers (including Firefox Nightly) but translates slowly, follow these instructions:

  1. Create a folder where you want to build all the artifacts (`build-wasm` in this case) and compile
      ```bash
      mkdir build-wasm
      cd build-wasm
      emcmake cmake -DCOMPILE_WASM=on ../
      emmake make -j2
      ```

  2. Patch generated artifacts to import GEMM library from a separate wasm module
       ```bash
       bash ../wasm/patch-artifacts-import-gemm-module.sh
       ```

#### Recompiling
As long as you don't update any submodule, just follow [Compile](#Compile) steps.\
If you update a submodule, execute following command in repository root folder before executing
[Compile](#Compile) steps.
```bash
git submodule update --init --recursive
```


## How to use

### Using Native version

The builds generate library that can be integrated to any project. All the public header files are specified in `src` folder.\
A short example of how to use the APIs is provided in `app/bergamot.cpp` file.

### Using WASM version

Please follow the `README` inside the `wasm` folder of this repository that demonstrates how to use the translator in JavaScript.

### Using python API

Compile and install:
```
export CMAKE_BUILD_PARALLEL_LEVEL=8 # Use 8 cores to compile
pip install wheel
pip install .

# Desktop app
% bergamot-translator --help
bergamot-translator interfance

options:
  -h, --help            show this help message and exit
  --config CONFIG, -c CONFIG
                        Model YML configuration input.
  --num-workers NUM_WORKERS, -n NUM_WORKERS
                        Number of CPU workers.
  --logging LOGGING, -l LOGGING
                        Set verbosity level of logging: trace, debug, info, warn, err(or), critical, off. Default is off
  --cache-size CACHE_SIZE
                        Cache size. 0 for caching is disabled
  --terminology-tsv TERMINOLOGY_TSV, -t TERMINOLOGY_TSV
                        Path to a terminology file TSV
  --force-terminology, -f
                        Force terminology to appear on the target side.
  --path-to-input PATH_TO_INPUT, -i PATH_TO_INPUT
                        Path to input file. Uses stdin if empty
```
Using the python interface
```python
from bergamot.translator import Translator
print(Translator.__doc__)
translator = Translator("/path/to/model.npz.best-bleu.npz.decoder.brg.yml", terminology="/path/to/terminology.tsv")
translator.translate(["text"])
[output]
new_terminology = {}
new_terminology['srcwrd'] = "trgwrd"
translator.reset_terminology(new_terminology)
translator.translate(["text"])
[output_with_terminology]
```
