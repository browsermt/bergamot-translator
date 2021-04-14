# Building marian code for bergamot

This document summarizes the minimal build instructions develop for the
marian-code powering bergamot-translator.

## Build Instructions

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
    --beam-size 1 --skip-cost --shortlist $MODEL_DIR/lex.s2t.gz 50 50 --int8shiftAlphaAll 

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

