# type: ignore
import pytest
from bergamot import REPOSITORY, ResponseOptions, Service, ServiceConfig, VectorString
from bergamot.utils import toJSON


def test_basic():
    keys = ["browsermt"]
    models = ["de-en-tiny"]
    config = ServiceConfig(numWorkers=1, logLevel="critical")
    service = Service(config)
    for repository in keys:
        # models = REPOSITORY.models(repository, filter_downloaded=False)
        for model in models:
            REPOSITORY.download(repository, model)

        for modelId in models:
            configPath = REPOSITORY.modelConfigPath(repository, modelId)
            model = service.modelFromConfigPath(configPath)
            options = ResponseOptions(alignment=True, qualityScores=True, HTML=False)
            print(repository, modelId)
            source = "1 2 3 4 5 6 7 8 9"
            responses = service.translate(model, VectorString([source]), options)
            for response in responses:
                print(toJSON(response, indent=4))
