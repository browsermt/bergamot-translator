Bergamot C++ Library
====================

This document contains instructions to develop for modifications on top
of the marian machine translation toolkit powering bergamot-translator.
The library is optimized towards fast and efficient translation of a
given input.

Build Instructions
------------------

Note: You are strongly advised to refer to the continuous integration on
this repository, which builds bergamot-translator and associated
applications from scratch. Examples to run these command
line-applications are available in the
`bergamot-translator-tests <https://github.com/browsermt/bergamot-translator-tests>`__
repository. Builds take about 30 mins on a consumer grade machine, so
using a tool like ccache is highly recommended.

Dependencies
~~~~~~~~~~~~

Marian CPU version requires Intel MKL or OpenBLAS. Both are free, but
MKL is not open-sourced. Intel MKL is strongly recommended as it is
faster. On Ubuntu 16.04 and newer it can be installed from the APT
repositories.

.. code:: bash

    wget -qO- 'https://apt.repos.intel.com/intel-gpg-keys/GPG-PUB-KEY-INTEL-SW-PRODUCTS-2019.PUB' | sudo apt-key add -
    sudo sh -c 'echo deb https://apt.repos.intel.com/mkl all main > /etc/apt/sources.list.d/intel-mkl.list'
    sudo apt-get update
    sudo apt-get install intel-mkl-64bit-2020.0-088

On MacOS, apple accelerate framework will be used instead of
MKL/OpenBLAS.

Building bergamot-translator
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Web Assembly (WASM) reduces building to only using a subset of
functionalities of marian, the translation library powering
bergamot-translator. When developing bergamot-translator it is important
that the sources added be compatible with marian. Therefore, it is
required to set ``-DUSE_WASM_COMPATIBLE_SOURCE=on``.

::

    $ git clone https://github.com/browsermt/bergamot-translator
    $ cd bergamot-translator
    $ mkdir build
    $ cd build
    $ cmake .. -DUSE_WASM_COMPATIBLE_SOURCE=off -DCMAKE_BUILD_TYPE=Release
    $ make -j2 

The build will generate the library that can be linked to any project.
All the public header files are specified in ``src`` folder.

Command line apps
-----------------

bergamot-translator is intended to be used as a library. However, we
provide a command-line application which is capable of translating text
provided on standard-input. During development this application is used
to perform regression-tests.


Example command line run
------------------------

The models required to run the command-line are available at
`data.statmt.org/bergamot/models/ <http://data.statmt.org/bergamot/models/>`__.

The following example uses an English to German tiny11 student model,
available at:

-  `data.statmt.org/bergamot/models/deen/ende.student.tiny11.tar.gz <http://data.statmt.org/bergamot/models/deen/ende.student.tiny11.tar.gz>`__

.. literalinclude:: ../examples/run-native.sh
   :language: bash

Coding Style
------------

This repository contains C++ and JS source-files, of which C++ should
adhere to the clang-format based style guidelines. You may configure
your development environment to use the ``.clang-format`` and
``.clang-format-ignore`` files provided in the root folder of this
repository with your preferred choice of editor/tooling.

One simple and recommended method to get your code to adhere to this
style is to issue the following command in the source-root of this
repository, which is used to also check for the coding style in the CI.

.. code:: bash

    python3 run-clang-format.py -i --style file -r src wasm
