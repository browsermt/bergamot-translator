# Bergamot Translator

Bergamot translator provides a unified API for ([Marian NMT](https://marian-nmt.github.io/) framework based) neural machine translation functionality in accordance with the [Bergamot](https://browser.mt/) project that focuses on improving client-side machine translation in a web browser.

## Build Instructions
### Build Natively
```
$ git clone https://github.com/browsermt/bergamot-translator
$ cd bergamot-translator
$ mkdir build
$ cd build
$ cmake ../
$ make -j

```

### Build on WASM
To compile on WASM, first download and Install Emscripten using following instructions:
```
Get the latest sdk: git clone https://github.com/emscripten-core/emsdk.git
Enter the cloned directory: cd emsdk
Install the lastest sdk tools: ./emsdk install latest
Activate the latest sdk tools: ./emsdk activate latest
Activate path variables: source ./emsdk_env.sh
```
After the successful installation of Emscripten, perform these steps:
```
$ git clone https://github.com/browsermt/bergamot-translator
$ cd bergamot-translator
$ mkdir build-wasm
$ cd build-wasm
$ emcmake cmake -DCOMPILE_WASM=on ../
$ emmake make -j
```
It should generate the artefacts (.js and .wasm files) in `wasm` folder inside build directory ("build-wasm" in this case).

After Editing Files
```
$ emmake make -j
```
After Adding/Removing Files
```
$ emcmake cmake -DCOMPILE_WASM=on ../
$ emmake make -j
```

## Using Bergamot Translator
### Using Native version
The builds generate library that can be integrated to any project. All the public header files are specified in `src` folder. A short example of how to use the APIs is provided in `app/main.cpp` file

### Using WASM version
Please follow the `README` inside the `wasm` folder of this respository that demonstrates how to use the translator in JavaScript.
