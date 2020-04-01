#!/bin/bash

. cmd.sh
. path.sh

train_nj=20
dir=./exp_pseudo/pseudo_use_train_mono_r5_d5_with_padding
train_data=./data_pseudo/train_r5_d5
lang=./lang_pseudo_50/lang


#utils/prepare_lang.sh --position-dependent-phones false --num-sil-states 5 --num-nonsil-states 5 lang_pseudo_50/dict "<unk>" lang_pseudo_50/temp lang_pseudo_50/lang

#LM=./data_1d/data_lang_1d/local/lm/lm.0gram.gz
#srilm_opts="-subset -prune-lowprobs -unk -tolower -order 1"
#utils/format_lm_sri.sh --srilm-opts "$srilm_opts" data_1d/data_lang_1d/lang_nosp $LM data_1d/data_lang_1d/local/dict_nosp/lexicon.txt data_1d/data_lang_1d/lang_nosp_0gram

steps/train_mono_no_delta.sh --totgauss 15500 --nj "$train_nj" --cmd "$train_cmd" $train_data $lang $dir  #sub-200

#steps/compute_cmvn_stats.sh data-dnn/competition exp/make_fea exp/make_fea_

#steps/train_mono_no_delta.sh --totgauss 1840000 --nj "$train_nj" --cmd "$train_cmd" $train_data $lang $dir  #new-7356


#LM=data/local/lm/lm.0gram.gz
#srilm_opts="-subset -prune-lowprobs -unk -tolower -order 1"
#utils/format_lm_sri.sh --srilm-opts "$srilm_opts" data/lang_4state $LM data/local/dict_nosp/lexicon.txt data/lang_4state_0gram

#graph_dir=/disk1/exp/mono_5_tied3/graph_0gram_true
#utils/mkgraph.sh --mono ./data/lang_nosp_0gram  /disk1/exp/mono_5_tied3 $graph_dir
