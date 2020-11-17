#!/bin/bash
# This script is used for monophone system training decoding in P2D situation
. ./cmd.sh 
[ -f path.sh ] && . ./path.sh

# bool options to decide whether to prepare_lang/train/decode or not
pre_lang=0
train=0
decode=0
zero=0

# train & decoding parameters
hidden_state_num=$1
train_tol_gauss=$2
train_nj=20
decode_nj=8
base_data_dir=./data_1d
train_data=${base_data_dir}/train
test_data=${base_data_dir}/test
#train_cmd=
#decode_cmd= 
base_lang_dir=./lang/lang_1d
lang=${base_lang_dir}/lang
lm=${base_lang_dir}/lm
LM=${base_lang_dir}/lm_1gram.gz
srilm_opts="-subset -prune-lowprobs -unk -tolower -order 1"
exp_dir=./exp_1d/${hidden_state_num}states_${train_tol_gauss}gauss
decode_dir=$exp_dir/decode

echo ============================================================================
echo "                     1D MonoPhone Training & Decoding                     "
echo ============================================================================

if [ $pre_lang -ne $zero ]; then
	utils/prepare_lang.sh --position-dependent-phones false \
		--num-sil-states 3 --num-nonsil-states ${hidden_state_num} \
		${base_lang_dir}/dict "<unk>" ${base_lang_dir}/temp $lang
	utils/format_lm_sri.sh --srilm-opts "$srilm_opts" ${lang} $LM ${base_lang_dir}/dict/lexicon.txt $lm
fi

if [ $train -ne $zero ]; then
	steps/train_mono_no_delta.sh --totgauss ${train_tol_gauss} \
		--nj "$train_nj" --cmd "$train_cmd" ${train_data} ${lang} ${exp_dir} 
fi

if [ $decode -ne $zero ]; then
	utils/mkgraph.sh --mono ${lm} ${exp_dir} ${exp_dir}/graph
	steps/decode.sh --nj "$decode_nj" --cmd "$decode_cmd" ${exp_dir}/graph ${test_data} ${decode_dir}
fi


