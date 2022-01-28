import json
import os
import re
import tarfile
import typing as t
from abc import ABC, abstractmethod
from functools import partial
from urllib.parse import urljoin, urlparse

import requests
import yaml
from appdirs import AppDirs

from .typing_utils import URL, PathLike
from .utils import download_resource, patch_marian_for_bergamot

APP = "bergamot"


class Repository(ABC):
    """
    An interface for several repositories. Intended to enable interchangable
    use of translateLocally and Mozilla repositories for usage through python.
    """

    @property
    @abstractmethod
    def name(self):
        pass

    @abstractmethod
    def update(self):
        """Updates the model list"""
        pass

    @abstractmethod
    def models(self) -> t.List[str]:
        """returns identifiers for available models"""
        pass

    @abstractmethod
    def model(self, model_identifier: str) -> t.Any:
        """returns entry for the  for available models"""
        pass

    @abstractmethod
    def modelConfigPath(self, model_identifier: str) -> str:
        """returns modelConfigPath for for a given model-identifier"""
        pass

    @abstractmethod
    def download(self, model_identifier: str):
        pass


class TranslateLocallyLike(Repository):
    """
    This class implements Repository to fetch models from translateLocally.
    AppDirs is used to standardize directories and further specialization
    happens with translateLocally identifier.
    """

    def __init__(self, name, url):
        self.url = url
        self._name = name
        appDir = AppDirs(APP)
        f = lambda *args: os.path.join(*args, self._name)
        self.dirs = {
            "cache": f(appDir.user_cache_dir),
            "config": f(appDir.user_config_dir),
            "data": f(appDir.user_data_dir),
            "archive": f(appDir.user_data_dir, "archives"),
            "models": f(appDir.user_data_dir, "models"),
        }

        for directory in self.dirs.values():
            os.makedirs(directory, exist_ok=True)

        self.models_file_path = os.path.join(self.dirs["config"], "models.json")
        self.update()

    @property
    def name(self) -> str:
        return self._name

    def update(self) -> None:
        inventory = requests.get(self.url).text
        with open(self.models_file_path, "w+") as models_file:
            models_file.write(inventory)
        self.data = json.loads(inventory)

        # Update inverse lookup.
        self.data_by_code = {}
        for model in self.data["models"]:
            self.data_by_code[model["code"]] = model

    def models(self, filter_downloaded: bool = True) -> t.List[str]:
        codes = []
        for model in self.data["models"]:
            if filter_downloaded:
                fprefix = self._archive_name_without_extension(model["url"])
                model_dir = os.path.join(self.dirs["models"], fprefix)
                if os.path.exists(model_dir):
                    codes.append(model["code"])
            else:
                codes.append(model["code"])
        return codes

    def modelConfigPath(self, model_identifier: str) -> str:
        model = self.model(model_identifier)
        fprefix = self._archive_name_without_extension(model["url"])
        model_dir = os.path.join(self.dirs["models"], fprefix)
        return os.path.join(model_dir, "config.bergamot.yml")

    def model(self, model_identifier: str) -> t.Any:
        return self.data_by_code[model_identifier]

    def download(self, model_identifier: str):
        # Download path
        model = self.model(model_identifier)
        model_archive = "{}.tar.gz".format(model["shortName"])
        save_location = os.path.join(self.dirs["archive"], model_archive)
        download_resource(model["url"], save_location)

        with tarfile.open(save_location) as model_archive:
            model_archive.extractall(self.dirs["models"])
            fprefix = self._archive_name_without_extension(model["url"])
            model_dir = os.path.join(self.dirs["models"], fprefix)
            symlink = os.path.join(self.dirs["models"], model["code"])

            print(
                "Downloading and extracting {} into ... {}".format(
                    model["code"], model_dir
                ),
                end=" ",
            )

            if not os.path.exists(symlink):
                os.symlink(model_dir, symlink)

            config_path = os.path.join(symlink, "config.intgemm8bitalpha.yml")
            bergamot_config_path = os.path.join(symlink, "config.bergamot.yml")

            # Finally patch so we don't have to reload this again.
            patch_marian_for_bergamot(config_path, bergamot_config_path)

            print("Done.")

    def _archive_name_without_extension(self, url: URL):
        o = urlparse(url)
        fname = os.path.basename(o.path)  # something tar.gz.
        fname_without_extension = fname.replace(".tar.gz", "")
        return fname_without_extension


class Mozilla(Repository):
    def __init__(self):
        self._name = "mozilla"
        appDir = AppDirs(APP)
        f = lambda *args: os.path.join(*args, self._name)
        self.dirs = {
            "cache": f(appDir.user_cache_dir),
            "config": f(appDir.user_config_dir),
            "data": f(appDir.user_data_dir),
            "archive": f(appDir.user_data_dir, "archives"),
            "models": f(appDir.user_data_dir, "models"),
        }

        for directory in self.dirs.values():
            os.makedirs(directory, exist_ok=True)

        self.inventory = {}
        self.update()

    def update(self):
        url = "https://gist.githubusercontent.com/jerinphilip/f9bc31d70e4201771b7734ab1cfd1205/raw/d2ddd8ddb822695f1ee2ccccbb12e9c0daeeabbf/mozilla-models.json"
        content = requests.get(url).text
        self.inventory = json.loads(content)

    def models(self, filter_downloaded=True):
        # Fakes a minimum required translateLocally entry
        # TODO(filter_downloaded)
        return list(self.inventory["modelRegistry"].keys())

    def model(self, model_identifier: str) -> t.Any:
        entry = self.inventory["modelRegistry"][model_identifier]
        dummy = {
            "modelName": model_identifier,
            "shortName": model_identifier,
            "type": "tiny",
            "src": "N/A",
            "trg": "N/A",
            "repository": "Mozilla",
            "version": 1,
            "API": 1,
            "checksum": "N/A",
            "url": "N/A",
            "name": model_identifier,
            "code": model_identifier,
        }
        return dummy

    def modelConfigPath(self, model_identifier: str) -> t.Any:
        model_dir = os.path.join(self.dirs["models"], model_identifier)
        return os.path.join(model_dir, "config.bergamot.yml")

    def download(self, model_identifier):
        payloads = self.inventory["modelRegistry"][model_identifier]
        model_dir = os.path.join(self.dirs["models"], model_identifier)
        os.makedirs(model_dir, exist_ok=True)
        for code, entry in payloads.items():
            fname = entry["name"]
            save_location = os.path.join(model_dir, fname)
            resource_url = "{}/{}".format(
                self.inventory["modelRegistryRootURL"],
                os.path.join(model_identifier, fname),
            )
            download_resource(resource_url, save_location)
        # Create a config-file
        configPath = os.path.join(model_dir, "config.bergamot.yml")

        config = {
            "models": [payloads["model"]["name"]],
            "vocabs": [payloads["vocab"]["name"], payloads["vocab"]["name"]],
            "shortlist": [payloads["lex"]["name"], False],
            "ssplit-prefix-file": "",
            "ssplit-mode": "paragraph",
            "max-length-break": 128,
            "mini-batch-words": 1024,
            "workspace": 128,  # shipped models use big workspaces. We'd prefer to keep it low.
            "alignment": "soft",
            "gemmPrecision": "int8ShiftAlphaAll",
        }

        print(config)

        with open(configPath, "w") as output_file:
            print(yaml.dump(config, sort_keys=False), file=output_file)

    @property
    def name(self):
        return self._name


class Aggregator:
    def __init__(self, repositories: t.List[Repository]):
        self.repositories = {}
        for repository in repositories:
            if repository.name in self.repositories:
                raise ValueError("Duplicate repository found.")
            self.repositories[repository.name] = repository

        # Default is self.repostiory
        self.default_repository = repositories[0]

    def update(self, name: str) -> None:
        self.repositories.get(name, self.default_repository).update()

    def modelConfigPath(self, name: str, code: str) -> PathLike:
        return self.repositories.get(name, self.default_repository).modelConfigPath(
            code
        )

    def models(self, name: str, filter_downloaded: bool = True) -> t.List[str]:
        return self.repositories.get(name, self.default_repository).models()

    def model(self, name: str, model_identifier: str) -> t.Any:
        return self.repositories.get(name, self.default_repository).model(
            model_identifier
        )

    def available(self):
        return list(self.repositories.keys())

    def download(self, name: str, model_identifier: str) -> None:
        self.repositories.get(name, self.default_repository).download(model_identifier)
