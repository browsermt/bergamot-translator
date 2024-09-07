import json
import os
import tarfile
import typing as t
from abc import ABC, abstractmethod
from functools import partial
from urllib.parse import urlparse

import requests
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
    def models(self, filter_downloaded: bool = True) -> t.List[str]:
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
        self.data = self._load_data(self.models_file_path)

        # Update inverse lookup.
        self.data_by_code = {}
        for model in self.data["models"]:
            self.data_by_code[model["code"]] = model

    @property
    def name(self) -> str:
        return self._name

    def _load_data(self, models_file_path):
        """
        Load model data from existing file. If file does not exist, download from the web.
        """
        if os.path.exists(models_file_path):
            # File already exists, prefer to work with this.
            # A user is expected to update manually if model's already
            # downloaded and setup.
            with open(models_file_path) as model_file:
                return json.load(model_file)
        else:
            # We are running for the first time.
            # Try to fetch this file from the internet.
            self.update()
            with open(models_file_path) as model_file:
                return json.load(model_file)

    def update(self) -> None:
        inventory = requests.get(self.url).text
        with open(self.models_file_path, "w+") as models_file:
            models_file.write(inventory)

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

            def is_within_directory(directory, target):
                abs_directory = os.path.abspath(directory)
                abs_target = os.path.abspath(target)

                prefix = os.path.commonprefix([abs_directory, abs_target])

                return prefix == abs_directory

            def safe_extract(tar, path=".", members=None, *, numeric_owner=False):
                for member in tar.getmembers():
                    member_path = os.path.join(path, member.name)
                    if not is_within_directory(path, member_path):
                        raise Exception("Attempted Path Traversal in Tar File")

                tar.extractall(path, members, numeric_owner=numeric_owner)

            safe_extract(model_archive, self.dirs["models"])
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
        fname_without_extension = ".".join(fname.split(".")[:3])
        return fname_without_extension


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
        return self.repositories.get(name, self.default_repository).models(filter_downloaded)

    def model(self, name: str, model_identifier: str) -> t.Any:
        return self.repositories.get(name, self.default_repository).model(
            model_identifier
        )

    def available(self):
        return list(self.repositories.keys())

    def download(self, name: str, model_identifier: str) -> None:
        self.repositories.get(name, self.default_repository).download(model_identifier)
