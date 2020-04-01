#!/bin/bash

. cmd.sh
. path.sh

train_nj=20
dir=./exp_true_2d/true_2d_r8_d8_11rows_after_force_state_of_lastrowcol
train_data=./data_true_2d/train_8r_8d
lang=./lang_true_2d/lang

#utils/prepare_lang.sh --position-dependent-phones false --num-sil-states 5 --num-nonsil-states 5 lang_true_2d/dict "<unk>" lang_true_2d/temp $lang

steps/train_mono_2d.sh --totgauss 23400 --nj "$train_nj" --cmd "$train_cmd" $train_data $lang $dir  #sub_50
