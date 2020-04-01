#!/bin/bash
# Copyright 2010-2013 Microsoft Corporation 
#                     Johns Hopkins University (Author: Daniel Povey)

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#  http://www.apache.org/licenses/LICENSE-2.0
#
# THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION ANY IMPLIED
# WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR PURPOSE,
# MERCHANTABLITY OR NON-INFRINGEMENT.
# See the Apache 2 License for the specific language governing permissions and
# limitations under the License.

split_per_spk=true		#默认  按说话人  划分
if [ "$1" == "--per-utt" ]; then
  split_per_spk=false
  shift
fi

if [ $# != 2 ]; then
  echo "Usage: split_data.sh [--per-utt] <data-dir> <num-to-split>"
  echo "This script will not split the data-dir if it detects that the output is newer than the input."
  echo "By default it splits per speaker (so each speaker is in only one split dir),"
  echo "but with the --per-utt option it will ignore the speaker information while splitting."
  exit 1
fi

data=$1                          #数据文件夹
numsplit=$2                      #切分的数目

if [ $numsplit -le 0 ]; then
  echo "Invalid num-split argument $numsplit";
  exit 1;
fi

n=0;
feats=""
wavs=""
utt2spks=""
texts=""
blocks=""

nu=`cat $data/utt2spk | wc -l`  # 存放utt2spk文件行数（语料数）
nf=`cat $data/feats.scp 2>/dev/null | wc -l` # 同上，2>/dev/null忽略错误信息，若无则会提示错误信息，但错误信息不会存入nf变量中
nt=`cat $data/text 2>/dev/null | wc -l` # take it as zero if no such file
# ---------- new line inserted for 2D ---------- #
nb=`cat $data/block 2>/dev/null | wc -l` # take it as zero if no such file
if [ -f $data/feats.scp ] && [ $nu -ne $nf ]; then
  echo "** split_data.sh: warning, #lines of (utt2spk,feats.scp) is ($nu,$nf); you can "
  echo "**  use utils/fix_data_dir.sh $data to fix this."
fi
if [ -f $data/text ] && [ $nu -ne $nt ]; then
  echo "** split_data.sh: warning, #lines is (utt2spk,text) is ($nu,$nt); you can "
  echo "** use utils/fix_data_dir.sh to fix this."
fi
# ---------- new code block inserted for 2D ---------- #
if [ -f $data/block ] && [ $nu -ne $nb ]; then
  echo "** split_data.sh: warning, #lines is (utt2spk,block) is ($nu,$nb); you can "
  echo "** use utils/fix_data_dir.sh to fix this."
fi

# 检测是否已经以nj=numsplit分割过任务
s1=$data/split$numsplit/1
if [ ! -d $s1 ]; then 
  need_to_split=true
else 
  need_to_split=false
  #wyn#for f in utt2spk spk2utt spk2warp feats.scp text wav.scp cmvn.scp spk2gender vad.scp segments reco2file_and_channel utt2lang; do
  # ---------- new name 'block' inserted for 2D ---------- #
  for f in utt2spk spk2utt feats.scp text cmvn.scp block; do
    if [[ -f $data/$f && ( ! -f $s1/$f || $s1/$f -ot $data/$f ) ]]; then
      need_to_split=true
    fi
  done
fi

#若已分割，则不再进行分割，退出脚本
if ! $need_to_split; then
  exit 0;
fi

#否则继续分割
for n in `seq $numsplit`; do # `seq n`表示1 2···n的数组
   mkdir -p $data/split$numsplit/$n
   utt2spks="$utt2spks $data/split$numsplit/$n/utt2spk" # 循环结束后utt2spks存放了所有的子utt2spk地址
done

if $split_per_spk; then
  utt2spk_opt="--utt2spk=$data/utt2spk"
else
  utt2spk_opt=
fi

# If lockfile is not installed, just don't lock it.  It's not a big deal.
which lockfile >&/dev/null && lockfile -l 60 $data/.split_lock 

#split_scp.pl脚本将$data/utt2spk均分到$utt2spks所列出来的numsplit个子文件中
utils/split_scp.pl $utt2spk_opt $data/utt2spk $utt2spks || exit 1

#由utt2spk生成对应的spk2utt
for n in `seq $numsplit`; do
  dsn=$data/split$numsplit/$n
  utils/utt2spk_to_spk2utt.pl $dsn/utt2spk > $dsn/spk2utt || exit 1;
done

maybe_wav_scp=
if [ ! -f $data/segments ]; then
  maybe_wav_scp=wav.scp  # If there is no segments file, then wav file is
                         # indexed per utt.
fi

# split some things that are indexed by utterance.
#for f in feats.scp text vad.scp utt2lang $maybe_wav_scp; do
# ---------- new name 'block' inserted for 2D ---------- #
for f in feats.scp text block; do
  if [ -f $data/$f ]; then
    utils/filter_scps.pl JOB=1:$numsplit \
      $data/split$numsplit/JOB/utt2spk $data/$f $data/split$numsplit/JOB/$f || exit 1;
  fi
done

# split some things that are indexed by speaker
#for f in spk2gender spk2warp cmvn.scp; do
for f in cmvn.scp; do
  if [ -f $data/$f ]; then
    utils/filter_scps.pl JOB=1:$numsplit \
      $data/split$numsplit/JOB/spk2utt $data/$f $data/split$numsplit/JOB/$f || exit 1;
  fi
done

for n in `seq $numsplit`; do
   dsn=$data/split$numsplit/$n
   if [ -f $data/segments ]; then
     utils/filter_scp.pl $dsn/utt2spk $data/segments > $dsn/segments
     awk '{print $2;}' $dsn/segments | sort | uniq > $data/tmp.reco # recording-ids.
     if [ -f $data/reco2file_and_channel ]; then
       utils/filter_scp.pl $data/tmp.reco $data/reco2file_and_channel > $dsn/reco2file_and_channel
     fi
     if [ -f $data/wav.scp ]; then
       utils/filter_scp.pl $data/tmp.reco $data/wav.scp >$dsn/wav.scp
     fi
     rm $data/tmp.reco
   fi # else it would have been handled above, see maybe_wav.
done

rm -f $data/.split_lock

exit 0
