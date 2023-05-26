#!/usr/bin/env python3
import bergamot
import argparse
from sys import stdin
from typing import List

class Translator:
    num_workers: int
    cache: int
    logging: str
    terminology: str
    force_terminology: bool

    config: bergamot.ServiceConfig
    model: bergamot.TranslationModel
    responseOpts: bergamot.ResponseOptions
    service: bergamot.Service

    def __init__(self, model_conifg_path: str, num_workers: int=1, cache: int=0, \
                 logging="off", terminology: str="", force_terminology: bool=False):
        self.num_workers = num_workers
        self.cache = cache
        self.logging = logging
        self.terminology = terminology
        self.force_terminology = force_terminology

        self.config = bergamot.ServiceConfig(self.num_workers, self.cache, self.logging, self.terminology, self.force_terminology)
        self.service = bergamot.Service(self.config)
        self.responseOpts = bergamot.ResponseOptions() # Default false for all, if we want to enable HTML later, from here
        self.translationModel = self.service.modelFromConfigPath(model_conifg_path)

    def resetTerminology(self, terminology: str="", force_terminology: bool=False) -> None:
        """Resets the terminology of the model"""
        self.terminology = terminology
        self.force_terminology = force_terminology
        self.config = bergamot.ServiceConfig(self.num_workers, self.cache, self.logging, self.terminology, self.force_terminology)
        self.service = bergamot.Service(self.config)

    def resetNumWorkers(self, num_workers) -> None:
        """Resets the number of workers"""
        self.num_workers = num_workers
        self.config = bergamot.ServiceConfig(self.num_workers, self.cache, self.logging, self.terminology, self.force_terminology)
        self.service = bergamot.Service(self.config)

    def translate(self, sentences: str) -> str:
        "Translates a strings"
        responses = self.service.translate(self.translationModel, bergamot.VectorString([sentences]), self.responseOpts)
        ret = ""
        for response in responses:
            ret = ret + response.target.text
        return ret
    
    def translate(self, sentences: List[str]) -> str:
        "Translates a list of strings"
        responses = self.service.translate(self.translationModel, bergamot.VectorString(sentences), self.responseOpts)
        ret = ""
        for response in responses:
            ret = ret + response.target.text
        return ret
    #@TODO add async translate with futures



if __name__ == '__main__':
    parser = argparse.ArgumentParser(description="bergamot-translator interfance")
    parser.add_argument("--config", '-c', required=True, type=str, help='Model YML configuration input.')
    parser.add_argument("--num-workers", '-n', type=int, default=1, help='Number of CPU workers.')
    parser.add_argument("--logging", '-l', type=str, default="off", help='Log level. Default is off')
    parser.add_argument("--cache-size", type=int, default=0, help='Cache size. 0 for caching is disabled')
    parser.add_argument("--terminology-tsv", '-t', default="", type=str, help='Path to a terminology file TSV')
    parser.add_argument("--force-terminology", '-f', action="store_true", help='Force terminology to appear on the target side.')
    parser.add_argument("--path-to-input", '-i', default=None, type=str, help="Path to input file. Uses stdin if empty")
    args = parser.parse_args()
    
    translator = Translator(args.config, args.num_workers, args.cache_size, args.logging, args.terminology_tsv, args.force_terminology)

    if args.path_to_input is not None:
        with open(args.path_to_input, 'r', encoding='utf-8') as infile:
            lines = infile.readlines()
            print(translator.translate(lines))
    else:
        for line in stdin:
            print(translator.translate(line))

