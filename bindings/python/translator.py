#!/usr/bin/env python3
import bergamot
import argparse
from sys import stdin
from typing import List, Dict

class Translator:
    """Bergamot translator interfacing with the C++ code.
    
    Attributes:
        num_workers        Number of parallel CPU workers.
        cache:             Cache size. 0 to disable cache.
        logging:           Log level: trace, debug, info, warn, err(or), critical, off. Default is off
        terminology:       Path to a TSV terminology file
        force_terminology  Force the terminology to appear on the target side. May affect translation quality negatively.
        
        _config            Translation model config
        _model:            Translation model
        _responseOpts      What to include in the response (alignment, html restoration, etc..)
        _service           The translation service
    """
    _num_workers: int
    _cache: int
    _logging: str
    _terminology: str
    _force_terminology: bool

    _config: bergamot.ServiceConfig
    _model: bergamot.TranslationModel
    _responseOpts: bergamot.ResponseOptions
    _service: bergamot.Service

    def __init__(self, model_conifg_path: str, num_workers: int=1, cache: int=0, \
                 logging="off", terminology: str="", force_terminology: bool=False):
        """Initialises the translator class

        :param model_conifg_path: Path to the configuration file for the translation model.
        :param num_workers: Number of CPU workers.
        :param cache: cache size. 0 means no cache.
        :param logging: Log level: trace, debug, info, warn, err(or), critical, off.
        :param terminology: Path to terminology file, TSV format
        :param force_terminology: Force terminology to appear on the target side. May impact translation quality.
        """
        self._num_workers = num_workers
        self._cache = cache
        self._logging = logging
        self._terminology = terminology
        self._force_terminology = force_terminology

        self._config = bergamot.ServiceConfig(self._num_workers, self._cache, self._logging, self._terminology, self._force_terminology)
        self._service = bergamot.Service(self._config)
        self._responseOpts = bergamot.ResponseOptions() # Default false for all, if we want to enable HTML later, from here
        self._model = self._service.modelFromConfigPath(model_conifg_path)

    def resetTerminology(self, terminology: str="", force_terminology: bool=False) -> None:
        """Resets the terminology of the model
        :param terminology: path to the terminology file.
        :param force_terminology: force terminology
        :return: None
        """
        self._terminology = terminology
        self._force_terminology = force_terminology
        self._config = bergamot.ServiceConfig(self._num_workers, self._cache, self._logging, self._terminology, self._force_terminology)
        self._service = bergamot.Service(self._config)

    def resetTerminology(self, terminology: Dict[str,str], force_terminology: bool=False) -> None:
        """Resets the terminology of the model
        :param terminology: Dictionary that maps source words to their target side terminology
        :param force_terminology: force terminology
        :return: None
        """
        self._service.setTerminology(terminology, force_terminology);

    def resetNumWorkers(self, num_workers) -> None:
        """Resets the number of workers
        :param num_workers: number of parallel CPU threads.
        :return: None
        """
        self._num_workers = num_workers
        self._config = bergamot.ServiceConfig(self._num_workers, self._cache, self._logging, self._terminology, self._force_terminology)
        self._service = bergamot.Service(self._config)
    
    def translate(self, sentences: List[str]) -> List[str]:
        """Translates a list of strings
        :param sentences: A List of strings to be translated.
        :return: A list of translation outputs.
        """
        responses = self._service.translate(self._model, bergamot.VectorString(sentences), self._responseOpts)
        return [response.target.text for response in responses]
    #@TODO add async translate with futures

def main():
    parser = argparse.ArgumentParser(description="bergamot-translator interface")
    parser.add_argument("--config", '-c', required=True, type=str, help='Model YML configuration input.')
    parser.add_argument("--num-workers", '-n', type=int, default=1, help='Number of CPU workers.')
    parser.add_argument("--logging", '-l', type=str, default="off", help='Set verbosity level of logging: trace, debug, info, warn, err(or), critical, off. Default is off')
    parser.add_argument("--cache-size", type=int, default=0, help='Cache size. 0 for caching is disabled')
    parser.add_argument("--terminology-tsv", '-t', default="", type=str, help='Path to a terminology file TSV')
    parser.add_argument("--force-terminology", '-f', action="store_true", help='Force terminology to appear on the target side.')
    parser.add_argument("--path-to-input", '-i', default=None, type=str, help="Path to input file. Uses stdin if empty")
    args = parser.parse_args()
    
    translator = Translator(args.config, args.num_workers, args.cache_size, args.logging, args.terminology_tsv, args.force_terminology)

    if args.path_to_input is not None:
        with open(args.path_to_input, 'r', encoding='utf-8') as infile:
            lines = infile.readlines()
            print("".join(translator.translate(lines)))
    else:
        for line in stdin:
            print("".join(translator.translate([line.strip()])))

if __name__ == '__main__':
    main()
