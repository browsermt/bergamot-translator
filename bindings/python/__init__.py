import typing

from ._bergamot import *  # type: ignore
from .repository import Aggregator, TranslateLocallyLike

REPOSITORY = Aggregator(
    [
        TranslateLocallyLike("browsermt", "https://translatelocally.com/models.json"),
        TranslateLocallyLike(
            "opus", "https://object.pouta.csc.fi/OPUS-MT-models/app/models.json"
        ),
    ]
)
"""
REPOSITORY is a global object that aggregates multiple model-providers to
provide a (model-provider: str, model-code: str) based query mechanism to
get models.
"""
