
DTM_FOLDER=$( realpath $(dirname $0)/../build/distilled)

# convert torch weights to marian 

python3 dp2marian.py --model "weights.th" --output "distilled.npz"

# run distilled model test conversion and save the results on a npz file

${DTM_FOLDER}/DistilledModel --model "distilled.npz" --output "distillied_marian_results.npz"

# compare the results with the deepQuest result npz file

python3 compare_marian_with_deepquest.py --marian "distillied_marian_results.npz" --dq "distillied_py_results.npz"
