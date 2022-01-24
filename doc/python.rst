.. Bergamot documentation master file, created by
   sphinx-quickstart on Tue Jan 18 17:26:57 2022.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

bergamot-translator
====================================

.. toctree::
   :maxdepth: 3
   :caption: Contents:


This document describes python bindings from bergamot-translator and a
batteries included python package supplied for easy use. The library also
provides entry point via a command-line making it easier for the average user
to get started.

As bergamot-translator is built on top of marian, the python API should also
work as python bindings for marian trained models, if they need to be
integrated into python code-bases.

*Disclaimer*: The package is still in early stages and unstable. Functions and
classes might move around quite fast. Use at your own risk.

Command Line Interface
----------------------

.. argparse::
   :ref: bergamot.cmds.make_parser
   :prog: bergamot


Module Documentation
--------------------

.. automodule:: bergamot
   :members:
   :undoc-members:
 
bergamot-translator
+++++++++++++++++++

The following components are exported from C++ via python-bindings and form
library primitives that can be used to build translation workflows.

.. autoclass:: bergamot.ServiceConfig
   :members:
   :undoc-members:

.. autoclass:: bergamot.Service
   :members:
   :undoc-members:


.. autoclass:: bergamot.TranslationModel
   :members:
   :undoc-members:

.. autoclass:: bergamot.ResponseOptions
   :members:
   :undoc-members:

Model Inventory
+++++++++++++++

.. autoclass:: bergamot.repository.Repository
   :members:
   :undoc-members:

.. autoclass:: bergamot.repository.TranslateLocallyLike
   :members:
   :undoc-members:

Utilities
+++++++++

.. autofunction:: bergamot.utils.patch_marian_for_bergamot



Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
