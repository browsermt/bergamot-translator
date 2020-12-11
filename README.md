# Bergamot Translator

Bergamot translator provides a unified API for ([Marian NMT](https://marian-nmt.github.io/) framework based) neural machine translation functionality in accordance with the [Bergamot](https://browser.mt/) project that focuses on improving client-side machine translation in a web browser.

## Build Instructions
```
$ git clone https://github.com/browsermt/bergamot-translator
$ cd bergamot-translator
$ mkdir build
$ cd build
$ cmake ../
$ make -j

```

## Using Bergamot Translator
The build will generate the library that can be linked to any project. All the public header files are specified in `src` folder.
