#!/bin/bash
# Author: jfma ustc
# Create for decoding under mono system in 2D situation
# This script is only suitable when each training sample only 
# has one phone in text, not for text-line modelling

config= # name of config file.
nj=8 # number of jobs,并行数
cmd=run.pl
stage=-4
norm_vars=false # deprecated, prefer --cmvn-opts "--norm-vars=false"
cmvn_opts=  # can be used to add extra options to cmvn.

echo "$0 $@"  # Print the command line for logging

if [ -f path.sh ]; then . ./path.sh; fi # path.sh用以import脚本路径，此后可以自由引用这些路径上的脚本
. parse_options.sh || exit 1;  # 解析训练使用的超参数（options）并改变之

if [ $# != 4 ]; then  # 如果实参个数不对，提示脚本正确用法并退出
  echo "Usage: steps/decode_2d.sh [options] <data-dir> <lang-dir> <model> <decode-dir>"
  echo " e.g.: steps/decode_2d.sh data/test data/lang exp/mono_2d/final.mdl exp/mono_2d/decode"
  echo "main options (for others, see top of script file)"
  echo "  --config <config-file>                           # config containing options"
  echo "  --nj <nj>                                        # number of parallel jobs"
  echo "  --cmd (utils/run.pl|utils/queue.pl <queue opts>) # how to run jobs."
  exit 1;
fi

data=$1
lang=$2
mdl=$3
dir=$4

# 按照任务数，将训练数据分成多份，每个任务处理一份数据。
mkdir -p $dir/log # -p选项自动创建尚不存在的目录
mkdir -p $dir/ali # -p选项自动创建尚不存在的目录
mkdir -p $dir/likely_phones # -p选项自动创建尚不存在的目录
mkdir -p $dir/wer # -p选项自动创建尚不存在的目录
echo $nj > $dir/num_jobs # num_jobs文件暂存并行数
sdata=$data/split$nj; # sdata为子任务存放目录

[[ -d $sdata && $data/feats.scp -ot $sdata ]] || split_data_2D.sh --per-utt $data $nj || exit 1;
$norm_vars && cmvn_opts="--norm-vars=false $cmvn_opts"
feats="ark:apply-cmvn $cmvn_opts --utt2spk=ark:$sdata/JOB/utt2spk scp:$sdata/JOB/cmvn.scp scp:$sdata/JOB/feats.scp ark:- |"
example_feats="`echo $feats | sed s/JOB/1/g`";
echo "Results will be saved in dir: $dir ..."

if [ $stage -le -3 ]; then
	echo "$0: Decoding samples ..."
	feat_dim=`feat-to-dim "$example_feats" - 2>/dev/null`
	[ -z "$feat_dim" ] && echo "error getting feature dimension" && exit 1;
	$cmd JOB=1:$nj $dir/log/decode.JOB.log \
		gmm-decode-2D "$mdl" "$feats" "ark:$sdata/JOB/block" "ark,t:$dir/ali/JOB.ali" "ark,t:$dir/likely_phones/JOB.phones_id" || exit 1;
fi

if [ $stage -le -2 ]; then
	echo "$0: Translate most likely phones_id sequence to most likely phones_name sequence ..."
	phone_id2phone.pl $lang/phones.txt $dir/likely_phones $dir/most_likely_phones.txt;
fi

if [ $stage -le -1 ]; then
	echo "$0: Calculating WER for $nj parallel jobs ..."
	$cmd JOB=1:$nj $dir/log/wer.JOB.log \
		gmm-wer-2D "ark:$dir/likely_phones/JOB.phones_id" \
		"ark:text2phone.pl $lang/../dict/lexicon.txt $lang/phones.txt $sdata/JOB/text|" \
	        "$dir/wer/JOB.wer" || exit 1;
fi

if [ $stage -le -0 ]; then
	echo "$0: Summarizing  WER ..."
	sum_wer.pl $dir/wer $dir/WER.txt;
fi

utils/summarize_warnings.pl $dir/log

echo Done
