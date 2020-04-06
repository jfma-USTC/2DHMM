#!/bin/bash
# Author: jfma ustc

if [ -f path.sh ]; then . ./path.sh; fi

gauss_per_state=2
state_num=156 # non-sil-state=3
log_file=wer_1d_3state.log

while [ ${gauss_per_state} -le 60 ]
do
	gauss=$[${gauss_per_state}*${state_num}]
	echo "running on state_non_sil=3, gauss_num=${gauss}"
	run_1d.sh ${gauss}
	exp_path=./exp_1d/temp_3state/3states_${gauss}gauss
	`echo ${gauss} >> ${log_file}`
	`cat ${exp_path}/decode/wer_* |grep WER|awk "{print $2}"|sort|head -1 >> ${log_file}`
	rm -rf ${exp_path}/log
	rm -rf ${exp_path}/decode/scoring
	gauss_per_state=$[${gauss_per_state}+2];
done
