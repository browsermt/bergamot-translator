#!/usr/bin/env python3
import bergamot
import argparse
from sys import stdin
from typing import List, Dict


class Translator:
    """Bergamot translator interfacing with the C++ code.

    Attributes:
        _num_workers        Number of parallel CPU workers.
        _gpu_workers        Indices of the GPU devices used. _num_workers must be set to zero!
        _cache:             Cache size. 0 to disable cache.
        _logging:           Log level: trace, debug, info, warn, err(or), critical, off. Default is off
        _terminology:       Path to a TSV terminology file
        _force_terminology  Force the terminology to appear on the target side. May affect translation quality negatively.
        _format             Format of the terminology string

        _config            Translation model config
        _model:            Translation model
        _responseOpts      What to include in the response (alignment, html restoration, etc..)
        _service           The translation service
    """

    _num_workers: int
    _gpu_workers: List[int]
    _cache: int
    _logging: str
    _terminology: str
    _force_terminology: bool
    _terminology_form: str

    _config: bergamot.ServiceConfig
    _model: bergamot.TranslationModel
    _responseOpts: bergamot.ResponseOptions
    _service: bergamot.Service

    def __init__(
        self,
        model_conifg_path: str,
        num_workers: int = 1,
        gpu_workers: List[int] = [],
        cache: int = 0,
        logging="off",
        terminology: str = "",
        force_terminology: bool = False,
        terminology_form: str = "%s __target__ %s __done__ ",
    ):
        """Initialises the translator class

        :param model_conifg_path: Path to the configuration file for the translation model.
        :param num_workers: Number of CPU workers.
        :param gpu_workers: Indices of the GPU devices. num_workers must be zero if this is non-empty
        :param cache: cache size. 0 means no cache.
        :param logging: Log level: trace, debug, info, warn, err(or), critical, off.
        :param terminology: Path to terminology file, TSV format
        :param force_terminology: Force terminology to appear on the target side. May impact translation quality.
        """
        self._num_workers = num_workers
        self._gpu_workers = gpu_workers
        self._cache = cache
        self._logging = logging
        self._terminology = terminology
        self._force_terminology = force_terminology
        self._terminology_form = terminology_form

        self._config = bergamot.ServiceConfig(
            self._num_workers,
            bergamot.VectorSizeT(self._gpu_workers),
            self._cache,
            self._logging,
            self._terminology,
            self._force_terminology,
            self._terminology_form,
        )
        self._service = bergamot.Service(self._config)
        self._responseOpts = (
            bergamot.ResponseOptions()
        )  # Default false for all, if we want to enable HTML later, from here
        self._model = self._service.modelFromConfigPath(model_conifg_path)

    def reset_terminology(
        self, terminology: str = "", force_terminology: bool = False
    ) -> None:
        """Resets the terminology of the model
        :param terminology: path to the terminology file.
        :param force_terminology: force terminology
        :return: None
        """
        self._terminology = terminology
        self._force_terminology = force_terminology
        self._config = bergamot.ServiceConfig(
            self._num_workers,
            bergamot.VectorSizeT(self._gpu_workers),
            self._cache,
            self._logging,
            self._terminology,
            self._force_terminology,
            self._terminology_form,
        )
        self._service = bergamot.Service(self._config)

    def reset_terminology(
        self, terminology: Dict[str, str], force_terminology: bool = False
    ) -> None:
        """Resets the terminology of the model
        :param terminology: Dictionary that maps source words to their target side terminology
        :param force_terminology: force terminology
        :return: None
        """
        self._service.setTerminology(terminology, force_terminology)

    def reset_num_workers(self, num_workers) -> None:
        """Resets the number of workers
        :param num_workers: number of parallel CPU threads.
        :return: None
        """
        self._num_workers = num_workers
        self._config = bergamot.ServiceConfig(
            self._num_workers,
            bergamot.VectorSizeT(self._gpu_workers),
            self._cache,
            self._logging,
            self._terminology,
            self._force_terminology,
            self._terminology_form,
        )
        self._service = bergamot.Service(self._config)

    def reset_gpu_workers(self, gpu_workers: List[int]) -> None:
        """Resets the number of GPU workers
        :param gpu_workers: Indices of the GPU devices to be used.
        :return: None
        """
        self._gpu_workers = gpu_workers
        self._config = bergamot.ServiceConfig(
            self._num_workers,
            bergamot.VectorSizeT(self._gpu_workers),
            self._cache,
            self._logging,
            self._terminology,
            self._force_terminology,
            self._terminology_form,
        )
        self._service = bergamot.Service(self._config)

    def translate(self, sentences: List[str]) -> List[str]:
        """Translates a list of strings
        :param sentences: A List of strings to be translated.
        :return: A list of translation outputs.
        """
        responses = self._service.translate(
            self._model, bergamot.VectorString(sentences), self._responseOpts
        )
        return [response.target.text for response in responses]

    # @TODO add async translate with futures


def main():
    parser = argparse.ArgumentParser(description="bergamot-translator interface")
    parser.add_argument("--config", '-c', required=True, type=str, help='Model YML configuration input.')
    parser.add_argument("--num-workers", '-n', type=int, default=1, help='Number of CPU workers.')
    parser.add_argument("--num-gpus", "-g", type=int, action='append', nargs='+', default=None, help='List of GPUs to use.')
    parser.add_argument("--logging", '-l', type=str, default="off", help='Set verbosity level of logging: trace, debug, info, warn, err(or), critical, off. Default is off')
    parser.add_argument("--cache-size", type=int, default=0, help='Cache size. 0 for caching is disabled')
    parser.add_argument("--terminology-tsv", '-t', default="", type=str, help='Path to a terminology file TSV')
    parser.add_argument("--force-terminology", '-f', action="store_true", help='Force terminology to appear on the target side.')
    parser.add_argument("--terminology-form", type=str, default="%s __target__ %s __done__ ", help='"Form for terminology. Default is "%%s __target__ %%s __done__ "')
    parser.add_argument("--path-to-input", '-i', default=None, type=str, help="Path to input file. Uses stdin if empty")
    parser.add_argument("--batch", '-b', default=32, type=int, help="Number of lines to process in a batch")
    args = parser.parse_args()

    if args.num_gpus is None:
        num_gpus = []
    else:
        num_gpus = args.num_gpus[0]
    translator = Translator(args.config, args.num_workers, num_gpus, args.cache_size, args.logging, args.terminology_tsv, args.force_terminology, args.terminology_form)


    if args.path_to_input is None:
        infile = stdin
    else:
        infile = open(args.path_to_input, "r", encoding="utf-8")

    # In this example, each block of input text (i.e. a document) is a line.
    # If you're using the API directly, feel free to include newlines in the
    # block of text.  We aim to preserve whitespace at sentence boundaries.

    # Buffer input text to allow the backend to parallelize.  We recommend
    # there be about 16 sentences per worker (thread).  Note that blocks of
    # text are internally split into sentences, so the number of sentences is
    # typically larger than the length of the list of blocks provided.
    buffer = []
    for line in infile:
        buffer.append(line.strip())
        if len(buffer) >= args.batch:
            print("\n".join(translator.translate(buffer)))
            buffer = []

    # Flush buffer
    if len(buffer) > 0:
        print("\n".join(translator.translate(buffer)))

    if args.path_to_input is not None:
        infile.close()


if __name__ == "__main__":
    main()
