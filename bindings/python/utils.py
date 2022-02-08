import os

import requests
import yaml

from .typing_utils import URL, PathLike


def download_resource(url: URL, save_location: PathLike, force_download=False):
    """
    Downloads a resource from url into save_location, overwrites only if
    force_download is true.
    """
    if force_download or not os.path.exists(save_location):
        response = requests.get(url, stream=True)
        # Throw an error for bad status codes
        response.raise_for_status()
        with open(save_location, "wb") as handle:
            for block in response.iter_content(1024):
                handle.write(block)


def patch_marian_for_bergamot(
    marian_config_path: PathLike, bergamot_config_path: PathLike, quality: bool = False
):
    """
    Accepts path to a config-file from marian-training and followign
    quantization and adjusts parameters for use in bergamot.
    """
    # Load marian_config_path
    data = None
    with open(marian_config_path) as fp:
        data = yaml.load(fp, Loader=yaml.FullLoader)

    # Update a few entries. Things here are hardcode.
    data.update(
        {
            "ssplit-prefix-file": "",
            "ssplit-mode": "paragraph",
            "max-length-break": 128,
            "mini-batch-words": 1024,
            "workspace": 128,  # shipped models use big workspaces. We'd prefer to keep it low.
            "alignment": "soft",
        }
    )

    if quality:
        data.update({"quality": quality, "skip-cost": False})

    # Write-out.
    with open(bergamot_config_path, "w") as output_file:
        print(yaml.dump(data, sort_keys=False), file=output_file)
