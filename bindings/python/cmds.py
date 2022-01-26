import argparse
import sys
from collections import Counter, defaultdict

from . import REPOSITORY, ResponseOptions, Service, ServiceConfig, VectorString

CMDS = {}


def _register_cmd(cmd: str):
    """
    Convenience decorator function, which populates the dictionary above with
    commands created in a declarative fashion.
    """

    def __inner(cls):
        CMDS[cmd] = cls
        return cls

    return __inner


@_register_cmd("translate")
class Translate:
    @staticmethod
    def embed_subparser(key: str, subparsers: argparse._SubParsersAction):
        translate = subparsers.add_parser(
            key,
            description="translate using a given model. Multiple models mean pivoting",
        )

        translate.add_argument(
            "-m",
            "--model",
            type=str,
            nargs="+",
            help="Path to model file(s) to use in forward or pivot translation",
            required=True,
        )

        translate.add_argument(
            "-r",
            "--repository",
            type=str,
            help="Repository to download model from",
            choices=REPOSITORY.available(),
            default="browsermt",
        )

        translate.add_argument(
            "--num-workers",
            type=int,
            help="Number of worker threads to use to translate",
            default=4,
        )

        translate.add_argument(
            "--log-level",
            type=str,
            default="off",
            help="Set verbosity level of logging: trace, debug, info, warn, err(or), critical, off",
        )

        # Tweak response-options for quick HTML in out via commandline
        options = translate.add_argument_group("response-options")
        options.add_argument("--html", type=bool, default=False)
        options.add_argument("--alignment", type=bool, default=False)
        options.add_argument("--quality-scores", type=bool, default=False)

    @staticmethod
    def execute(args: argparse.Namespace):
        # Build service

        config = ServiceConfig(numWorkers=args.num_workers, logLevel=args.log_level)
        service = Service(config)

        models = [
            service.modelFromConfigPath(
                REPOSITORY.modelConfigPath(args.repository, model)
            )
            for model in args.model
        ]

        # Configure a few options which require how a Response is constructed
        options = ResponseOptions(
            alignment=args.alignment, qualityScores=args.quality_scores, HTML=args.html
        )

        source = sys.stdin.read()
        responses = None
        if len(models) == 1:
            [model] = models
            responses = service.translate(model, VectorString([source]), options)
        else:
            [first, second] = models
            responses = service.pivot(first, second, VectorString([source]), options)

        for response in responses:
            print(response.target.text, end="")


@_register_cmd("download")
class Download:
    @staticmethod
    def embed_subparser(key: str, subparsers: argparse._SubParsersAction):
        download = subparsers.add_parser(
            key, description="Download models from the web."
        )

        download.add_argument(
            "-m",
            "--model",
            type=str,
            required=False,
            default=None,
            help="Fetch model with given code. Use ls to list available models. Optional, if none supplied all models are downloaded.",
        )

        download.add_argument(
            "-r",
            "--repository",
            type=str,
            help="Repository to download model from",
            choices=REPOSITORY.available(),
            default="browsermt",
        )

    @staticmethod
    def execute(args: argparse.Namespace):
        if args.model is not None:
            REPOSITORY.download(args.repository, args.model)
        else:
            for model in REPOSITORY.models(args.repository, filter_downloaded=False):
                REPOSITORY.download(args.repository, model)


@_register_cmd("ls")
class List:
    @staticmethod
    def embed_subparser(key: str, subparsers: argparse._SubParsersAction):
        ls = subparsers.add_parser(key, description="List available models.")
        ls.add_argument(
            "-r",
            "--repository",
            type=str,
            help="Repository to list models from",
            choices=REPOSITORY.available(),
            default="browsermt",
        )

    @staticmethod
    def execute(args: argparse.Namespace):
        print("Available models: ")
        for counter, identifier in enumerate(
            REPOSITORY.models(args.repository, filter_downloaded=True), 1
        ):
            model = REPOSITORY.model(args.repository, identifier)
            print(
                " {}.".format(str(counter).rjust(4)),
                model["code"],
                model["name"],
            )
        print()


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser("bergamot")
    subparsers = parser.add_subparsers(
        title="actions",
        description="The following actions are available through the bergamot package",
        help="To obtain help on how to run these actions supply <cmd> -h.",
        dest="action",
    )

    for key, cls in CMDS.items():
        cls.embed_subparser(key, subparsers)
    return parser
