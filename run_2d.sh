#!/bin/bash
# This script is used for monophone system decoding in 2D situation
. ./cmd.sh 
[ -f path.sh ] && . ./path.sh

# bool options to decide whether to prepare_lang/train/decode or not
pre_lang=0
train=0
decode=1
zero=0

# train & decoding parameters
train_tol_gauss=23400
train_nj=20
decode_nj=8
train_data=./data_true_2d/train_8r_8d
test_data=./data_true_2d/test_8r_8d
#train_cmd=
#decode_cmd= 
topo_2d=./lang_true_2d/lang/topo_2d  # topo file for 2D situation
lang=./lang_true_2d/lang
exp_dir=./exp_true_2d/true_2d_r8_d8_11rows_after_force_state_of_lastrowcol
decode_dir=$exp_dir/decode

echo ============================================================================
echo "                     2D MonoPhone Training & Decoding                     "
echo ============================================================================

if [ $pre_lang -ne $zero ]; then
	utils/prepare_lang.sh --position-dependent-phones false --num-sil-states 5 --num-nonsil-states 5 \
		lang_true_2d/dict "<unk>" lang_true_2d/temp $lang
	if [ -f $topo_2d ]; then 
		cp $topo_2d $lang/topo
	else
		echo "$topo_2d not exist, please check first."
		exit 1;
	fi
fi

if [ $train -ne $zero ]; then
	steps/train_mono_2d.sh --totgauss $train_tol_gauss --nj "$train_nj" --cmd "$train_cmd" $train_data $lang $dir  #sub_50
fi

if [ $decode -ne $zero ]; then
	steps/decode_2d.sh --nj "$decode_nj" --cmd "$decode_cmd" $test_data $lang $exp_dir/final.mdl $decode_dir
fi
