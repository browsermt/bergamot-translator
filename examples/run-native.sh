# In source-root folder

# Obtain an example model from the web.
mkdir -p models
wget --quiet --continue --directory models/ \
    http://data.statmt.org/bergamot/models/deen/ende.student.tiny11.tar.gz 
tar -xzf models/ende.student.tiny11.tar.gz 

# Patch the config-files generated from marian for use in bergamot.
python3 bergamot-translator-tests/tools/patch-marian-for-bergamot.py \
    --config-path models/ende.student.tiny11/config.intgemm8bitalpha.yml \
    --ssplit-prefix-file 3rd-party/ssplit-cpp/split-cpp/nonbreaking_prefixes/nonbreaking_prefix.en

# Patched config file will be available with .bergamot.yml suffix.
CONFIG=models/ende.student.tiny11/config.intgemm8bitalpha.yml.bergamot.yml

build/app/bergamot --model-config-paths $CONFIG --cpu-threads 4 <<< "Hello World!"
# Hallo Welt!

