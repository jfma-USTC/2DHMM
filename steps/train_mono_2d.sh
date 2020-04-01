#!/bin/bash
# Author: jfma ustc
# Create for training mono system in 2D situation
# This script is only suitable when each training sample only 
# has one phone in text, not for text-line modelling

# To be run from ..
# Flat start and monophone training.
# This script applies cepstral mean normalization (per speaker).

nj=8 # number of jobs,并行数
cmd=run.pl
scale_opts="--transition-scale=1.0 --acoustic-scale=0.1 --self-loop-scale=0.1"
num_iters=40    # Number of iterations of training
max_iter_inc=5 # Last iter to increase #Gauss on.
totgauss=1000 # Target Gaussians. This should be changed according to phone-list size
#totgauss=1250000 # Target #Gaussians.  
careful=false
boost_silence=1.0 # Factor by which to boost silence likelihoods in alignment # 对齐时，对sil的likelihoods进行放大
realign_iters="1 2 3 4 5 6 7 8 9 10 12 14 16 18 20 23 26 29 32 35 38"; # 只在指定迭代轮数进行标签的重对齐
config= # name of config file.
stage=-4
power=0.25 # exponent to determine number of gaussians from occurrence counts
norm_vars=false # deprecated, prefer --cmvn-opts "--norm-vars=false"
cmvn_opts=  # can be used to add extra options to cmvn.
# End configuration section.

echo "$0 $@"  # Print the command line for logging

if [ -f path.sh ]; then . ./path.sh; fi # path.sh用以import脚本路径，此后可以自由引用这些路径上的脚本
. parse_options.sh || exit 1;  # 解析训练使用的超参数（options）并改变之

if [ $# != 3 ]; then  # 如果实参个数不对，提示脚本正确用法并退出
  echo "Usage: steps/train_mono_2d_one_char.sh [options] <data-dir> <lang-dir> <exp-dir>"
  echo " e.g.: steps/train_mono_2d_one_char.sh data/train data/lang exp/mono"
  echo "main options (for others, see top of script file)"
  echo "  --config <config-file>                           # config containing options"
  echo "  --nj <nj>                                        # number of parallel jobs"
  echo "  --cmd (utils/run.pl|utils/queue.pl <queue opts>) # how to run jobs."
  exit 1;
fi

data=$1
lang=$2
dir=$3

oov_sym=`cat $lang/oov.int` || exit 1; # 解析反引号内的文法

# 按照任务数，将训练数据分成多份，每个任务处理一份数据。
mkdir -p $dir/log # -p选项自动创建尚不存在的目录
echo $nj > $dir/num_jobs # num_jobs文件暂存并行数
sdata=$data/split$nj; # sdata为子任务存放目录
echo "yes"
[[ -d $sdata && $data/feats.scp -ot $sdata ]] || split_data_2D.sh --per-utt $data $nj || exit 1;
$norm_vars && cmvn_opts="--norm-vars=false $cmvn_opts"
feats="ark:apply-cmvn $cmvn_opts --utt2spk=ark:$sdata/JOB/utt2spk scp:$sdata/JOB/cmvn.scp scp:$sdata/JOB/feats.scp ark:- |"
example_feats="`echo $feats | sed s/JOB/1/g`";
echo $example_feats
echo "$0: Initializing monophone system."

if [ $stage -le -3 ]; then
  feat_dim=`feat-to-dim "$example_feats" - 2>/dev/null`
  echo "featdim : $feat_dim"
  [ -z "$feat_dim" ] && echo "error getting feature dimension" && exit 1;
  $cmd JOB=1 $dir/log/init.log \
	gmm-init-mono-2D "--train-feats=$feats subset-feats --n=10 ark:- ark:-|" $lang/topo $feat_dim \
    $dir/0.mdl || exit 1;
fi

numgauss=`gmm-info-2D --print-args=false $dir/0.mdl | grep gaussians | awk '{print $NF}'`
echo $numgauss
incgauss=$[($totgauss-$numgauss)/$max_iter_inc] # per-iter increment for #Gauss
echo "incgauss is $incgauss"

if [ $stage -le -1 ]; then
  echo "$0: Aligning data equally (pass 0)"
  $cmd JOB=1:$nj $dir/log/align.0.JOB.log \
    align-equal-2D $dir/0.mdl  ark:$sdata/JOB/block "ark:text2phone.pl $lang/../dict/lexicon.txt $lang/phones.txt $sdata/JOB/text|" ark,t:- \| \
		gmm-acc-stats-ali-2D --binary=true $dir/0.mdl "$feats" \
		ark:$sdata/JOB/block ark:-  $dir/0.JOB.acc || exit 1;
fi

if [ $stage -le 0 ]; then
  gmm-est-2D --min-gaussian-occupancy=3  --mix-up=$numgauss --power=$power \
    $dir/0.mdl "gmm-sum-accs-2D - $dir/0.*.acc|"\
	$dir/1.mdl 2> $dir/log/update.0.log || exit 1;
  rm $dir/0.*.acc
fi


x=1
while [ $x -lt $num_iters ]; do
  echo "$0: Pass $x"
  if [ $stage -le $x ]; then
    if echo $realign_iters | grep -w $x >/dev/null; then
      echo "$0: Aligning data"
	  mdl="$dir/$x.mdl"
      $cmd JOB=1:$nj $dir/log/align.$x.JOB.log \
        gmm-align-2D "$mdl" "$feats" ark:$sdata/JOB/block\
		"ark:text2phone.pl $lang/../dict/lexicon.txt $lang/phones.txt $sdata/JOB/text|"\
		"ark,t:|gzip -c >$dir/ali.JOB.gz"\
        || exit 1;
    fi
    $cmd JOB=1:$nj $dir/log/acc.$x.JOB.log \
      gmm-acc-stats-ali-2D  $dir/$x.mdl "$feats" ark:$sdata/JOB/block \
	  "ark:gunzip -c $dir/ali.JOB.gz|" \
      $dir/$x.JOB.acc || exit 1;

    $cmd $dir/log/update.$x.log \
      gmm-est-2D --write-occs=$dir/$[$x+1].occs --mix-up=$numgauss --power=$power $dir/$x.mdl \
      "gmm-sum-accs-2D - $dir/$x.*.acc|" $dir/$[$x+1].mdl || exit 1;
    rm $dir/$x.mdl $dir/$x.*.acc $dir/$x.occs 2>/dev/null
  fi
  if [ $x -le $max_iter_inc ]; then
     numgauss=$[$numgauss+$incgauss];
  fi
  x=$[$x+1]
done

( cd $dir; rm final.{mdl,occs} 2>/dev/null; ln -s $x.mdl final.mdl; ln -s $x.occs final.occs )

utils/summarize_warnings.pl $dir/log

echo Done
