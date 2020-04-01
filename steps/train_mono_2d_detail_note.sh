#!/bin/bash
# Author: jfma ustc
# Create for training mono system in 2D situation
# This script is only suitable when each training sample only 
# has one phone in text, not for text-line modelling

# To be run from ..
# Flat start and monophone training.
# This script applies cepstral mean normalization (per speaker).

# 这一部分是训练使用的参数，可以通过命令行追加 –name value的方式改变
# Begin configuration section.
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
  echo "Usage: steps/train_mono_2d.sh [options] <data-dir> <lang-dir> <exp-dir>"
  echo " e.g.: steps/train_mono_2d.sh data/train data/lang exp/mono"
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
#[[ -d $sdata && $data/feats.scp -ot $sdata ]] || split_data.sh $data $nj || exit 1;
[[ -d $sdata && $data/feats.scp -ot $sdata ]] || split_data_2D.sh --per-utt $data $nj || exit 1;
# 选项--per-utt指按语音划分，而非按照说话人划分，此时是按feats.scp行数依次均分20份给不同的/split20/JOB/feat
$norm_vars && cmvn_opts="--norm-vars=false $cmvn_opts"

# 原始train_mono.sh:
# feats="ark,s,cs:apply-cmvn $cmvn_opts --utt2spk=ark:$sdata/JOB/utt2spk scp:$sdata/JOB/cmvn.scp scp:$sdata/JOB/feats.scp ark:- |add-deltas ark:- ark:- |"
# apply-cmvn  对feat.scp提取特征的CMVN，即为倒谱方差均值归一化
# 3个输入文件：utt2spk(发音id 说话人), cmvn.scp(说话人相关的统计量), feats.scp(训练用的特征文件)
# 输出是 ark:-|，利用管道技术把输出传递给下一个函数作为输入 
# add-deltas 输入是ark:-，训练数据增加差分量，比如13维度mfcc处理后变成39维度
# ORIGIN # feats="ark,s,cs:apply-cmvn $cmvn_opts --utt2spk=ark:$sdata/JOB/utt2spk scp:$sdata/JOB/cmvn.scp scp:$sdata/JOB/feats.scp ark:- | add-deltas ark:- ark:- |"
# TEST # feats="copy-feats scp:$sdata/JOB/feats.scp ark:- |"
feats="ark:apply-cmvn $cmvn_opts --utt2spk=ark:$sdata/JOB/utt2spk scp:$sdata/JOB/cmvn.scp scp:$sdata/JOB/feats.scp ark:- |"
echo $feats
example_feats="`echo $feats | sed s/JOB/1/g`";
# 建议使用consolas字体显示，这里是反引号`，`~`中的内容首先被执行，执行结果作为表达式的值
echo $example_feats   # example_feats内容就是  copy-feats scp:$data/1/feats.scp ark:- |
echo "$0: Initializing monophone system."

# ORIGIN # [ ! -f $lang/phones/sets.int ] && exit 1; # sets.int 为音素列表文件，在这里为200行1~200的数字
# ORIGIN # shared_phones_opt="--shared-phones=$lang/phones/sets.int"

# TEST # feat_dim=`feat-to-dim scp:$sdata/1/feats.scp ark:- | - 2>/dev/null`

# 如果$stage小于等于-3，那么获取训练数据的特征维度并创建0.mdl和tree
if [ $stage -le -3 ]; then
# Note: JOB=1 just uses the 1st part of the features-- we only need a subset anyway.
# 获取特征的维度，得到$sdata/JOB/feats.scp维度的指令：
# 1、feat-to-dim scp:$sdata/JOB/feats.scp - 2>/dev/null 得到特征维度
# 2、copy-feats ark:fea_train_ark/cmvn_train.ark ark,t:- | sed -n '1,5p'  查看特征文件的1~5行
# 3、copy-feats ark:fea_train_ark/cmvn_train.ark ark,t:- | head -5 |awk '{print NF}'  打印特征文件前五行的字段数目（字段按空格或tab划分）
  feat_dim=`feat-to-dim "$example_feats" - 2>/dev/null`
  [ -z "$feat_dim" ] && echo "error getting feature dimension" && exit 1;
#	 此处$cmd=run.pl ,这由cmd.sh指定，run.pl作用见下：
#    * 常见用法：
#             run.pl some.log a b c
#        即在 bash 环境中执行 a b c 命令，并将日志输出到 some.log 文件中
#    * 并行任务：
#             run.pl JOB=1:4 some.JOB.log  a b c JOB
#        即在 bash 环境中执行 a b c JOB 命令，并将日志输出到 some.JOB.log 文件中, 其中 JOB 表示执行任务的名称， 任意一个 Job 失败，整体失败。
  # subset-feats：Copy a subset of features (by default, the first n feature files)
  $cmd JOB=1 $dir/log/init.log \
    # ORIGIN # gmm-init-mono-2D $shared_phones_opt "--train-feats=$feats subset-feats --n=10 ark:- ark:-|" $lang/topo $feat_dim \#
	gmm-init-mono-2D "--train-feats=$feats subset-feats --n=10 ark:- ark:-|" $lang/topo $feat_dim \
    $dir/0.mdl || exit 1;
	# ORIGIN # $dir/0.mdl $dir/tree || exit 1;
  # Flat-start（又称为快速启动），作用是利用少量的数据快速得到一个初始化的 HMM-GMM 模型和决策树
  # $lang/topo 中定义了每个音素（phone）所对应的 HMM 模型状态数以及初始时的转移概率
  # --shared-phones=$lang/phones/sets.int 选项指向的文件，即$lang/phones/sets.int(该文件生成roots.txt中开头为share split的部分，
  # 表示同一行元素共享pdf，允许进行决策树分裂),文件中同一行的音素（phone）共享 GMM 概率分布。tree文件由sets.int产生。
  # --train-feats=$feats subset-feats --n=10 ark:- ark:-| 选项指定用来初始化训练用的特征，一般采用少量数据
  # 通过 subset-feats 来取出feats中的前 10个文件的特征向量作为参数
  # 程序内部会计算这批数据的means和variance，作为初始高斯模型。sets.int中所有行的初始pdf都用这个计算出来的means和variance进行初始化。
fi

#获取高斯数 ： gmm-info 0.mdl 搜索 gaussians，输出最后1个字段的内容，等于所有pdf_id的weights_.size()之和，即总高斯数
#awk中   NF为字段的个数， $NF表示最后一个字段的内容，注意，NF前面有没有 $ 的结果完全不一样！
numgauss=`gmm-info-2D --print-args=false $dir/0.mdl | grep gaussians | awk '{print $NF}'`
# $totgauss = 目标高斯数(最终需要达到的总高斯数)
# $numgauss = 现阶段高斯数(在这里是pdf_id的数目，因为init-mono的时候每个pdf用的单高斯来刻画)
# $max_iter_inc = 一共需要在多少次迭代后达到目标高斯数
# $incgauss = 每次迭代的高斯数增量
incgauss=$[($totgauss-$numgauss)/$max_iter_inc] # per-iter increment for #Gauss
echo "incgauss is $incgauss"

# 如果$stage小于等于-2，那么
# 构造训练的网络，从源码级别分析，是每个句子构造一个phone level 的fst网络。
# $sdaba/JOB/text 中包含对每个句子的单词(words level)级别标注， L.fst是字典对于的fst表示，作用是将一串的音素（phones）转换成单词（words）
# 构造monophone解码图就是先将text中的每个句子，生成一个fst（类似于语言模型中的G.fst，只是相对比较简单，只有一个句子），
# 然后和L.fst 进行composition 形成训练用的音素级别（phone level）fst网络（类似于LG.fst）。
# fsts.JOB.gz 中使用 key-value 的方式保存每个句子和其对应的fst网络，通过 key(句子) 就能找到这个句子的fst网络，
# value中保存的是句子中每两个音素之间互联的边（Arc）,例如句子转换成音素后，标注为："a b c d e f",
# 那么value中保存的其实是 a->b b->c c->d d->e e->f 这些连接（kaldi会为每种连接赋予一个唯一的id），
# 后面进行 HMM 训练的时候是根据这些连接的id进行计数，就可以得到转移概率。

#----------------------originally exist, but currently not needed------------------------#
#if [ $stage -le -2 ]; then
#  echo "$0: Compiling training graphs"
#  $cmd JOB=1:$nj $dir/log/compile_graphs.JOB.log \
#    compile-train-graphs $dir/tree $dir/0.mdl  $lang/L.fst \
#    "ark:sym2int.pl --map-oov $oov_sym -f 2- $lang/words.txt < $sdata/JOB/text|" \
#    "ark:|gzip -c >$dir/fsts.JOB.gz" || exit 1;
#fi

# Usage: compile-train-graphs [options] <tree-in> <model-in> <lexicon-fst-in> <transcriptions-rspecifier> <graphs-wspecifier>
# 注意：这里"ark:sym2int.pl --map-oov $oov_sym -f 2- $lang/words.txt < $sdata/JOB/text|"对应的<transcriptions-rspecifier>格式如下：
# 0127-P18_6 2 4792 7134 6122 3936 6320 542 6516 2353 7281 6122 1673 2637 4322 2011 2544 5672 1175 857 5331 3876 2519 5241 6122 1673 2011 2544 2
# 1225-f_5496 2 1432 2
# 1286-f_706F 2 995 2
# 0559-f_956B 2 2829 2
# 0361-f_53E8 2 2125 2
# 此处 sym2int.pl 利用word.txt提供的word-index标记，将text【0392-f_8BF8 sil w4461 sil】转化为 int【0392-f_8BF8 2 3800 2】


if [ $stage -le -1 ]; then
  echo "$0: Aligning data equally (pass 0)"
  $cmd JOB=1:$nj $dir/log/align.0.JOB.log \
# 训练时需要将标注跟每一帧特征进行对齐，由于现在还没有可以用于对齐的模型，所以采用最简单的方法 -- 均匀对齐
# 根据标注数目对特征序列进行等间隔切分，例如一个具有5个标注的长度为100帧的特征序列，则认为1-20帧属于第1个标注，21-40属于第2个...
# 这种划分方法虽然会有误差，但待会在训练模型的过程中会不断地重新对齐。
# 2D情形下则是将state-map缩放到block-map，被覆盖到的block被赋予对应的state
	# ORIGIN # align-equal-compiled "ark:gunzip -c $dir/fsts.JOB.gz|" "$feats" ark,t:-  \| \
    align-equal-2D $dir/0.mdl "ark:sym2int.pl --map-oov $oov_sym -f 2- $lang/words.txt < $sdata/JOB/text|"  ark:$sdata/JOB/block\
	# 这里的sym2int.pl输出的信息为将text中的标注信息【symbol】转化为音素序号【intger】
	# 未见词对应oov，-f指定范围，2-指从每行第二个字符开始进行转换
	# 输出为 file_name phone1 ... phoneN
		"ark,t:|gzip -c >$dir/ali.JOB.gz";
	# 此处：feats="copy-feats scp:$sdata/JOB/feats.scp ark:- |"
# 对对齐后的数据进行训练，获得中间统计量，每个任务输出到一个acc文件。
# acc中记录跟HMM 和GMM 训练相关的统计量：
# HMM 相关的统计量：两个音素之间互联的边（Arc） 出现的次数。
# 	如上面所述，fst.JOB.gz 中每个key对于的value保存一个句子中音素两两之间互联的边。
#	gmm-acc-stats-ali 会统计每条边（例如a->b）出现的次数，然后记录到acc文件中。
# GMM 相关的统计量：每个pdf-id 对应的特征累计值和特征平方累计值。
#	对于每一帧，都会有个对齐后的标注，gmm-acc-stats-ali 可以根据标注检索得到pdf-id,
# 	每个pdf-id 对应的GMM可能由多个单高斯Component组成，会先计算在每个单高斯Component对应的分布下
# 	这一帧特征的似然概率（log-likes），称为posterior。
# 	然后：
# 		（1）把每个单高斯Component的posterior加到每个高斯Component的occupancy（占有率）计数器上，用于表征特征对于高斯的贡献度，
#            如果特征一直落在某个高斯的分布区间内，那对应的这个值就比较大；相反，如果一直落在区间外，则表示该高斯作用不大。
#            gmm-est中可以设置一个阈值，如果某个高斯的这个值低于阈值，则不更新其对应的高斯。
#            另外这个值（向量)其实跟后面GMM更新时候的高斯权重weight的计算相关。
#		（2）把这一帧数据加上每个单高斯Component的posterior再加到每个高斯的均值累计值上；
#            这个值（向量）跟后面GMM的均值更新相关。
#		（3）把这一帧数据的平方值加上posterior再加到每个单高斯Component的平方累计值上；
#            这个值（向量）跟后面GMM的方差更新相关。
#            最后将均值累计值和平方累计值写入到文件中。
	# ORIGIN # gmm-acc-stats-ali --binary=true $dir/0.mdl "$feats" ark:- \
    # ORIGIN # $dir/0.JOB.acc || exit 1;
    gmm-acc-stats-ali-2D --binary=true $dir/0.mdl "$feats" \
		ark:$sdata/JOB/block "ark:gunzip -c $dir/ali.JOB.gz|" \
		$dir/0.JOB.acc || exit 1;
fi

# In the following steps, the --min-gaussian-occupancy=3 option is important, otherwise
# we fail to est "rare" phones and later on, they never align properly.
# 根据上面得到的统计量，更新每个GMM模型，AccumDiagGmm中occupancy_的值决定混合高斯模型中每个单高斯Component的weight；
# --min-gaussian-occupancy 的作用是设置occupancy_的阈值，如果某个单高斯Component的occupancy_低于这个阈值，那么就不会更新这个高斯，
# 而且如果 --remove-low-count-gaussians=true,则对应得单高斯Component会被移除。
if [ $stage -le 0 ]; then
  gmm-est-2D --min-gaussian-occupancy=3  --mix-up=$numgauss --power=$power \
    # ORIGIN # $dir/0.mdl "gmm-sum-accs - $dir/0.*.acc|" $dir/1.mdl 2> $dir/log/update.0.log || exit 1;
    $dir/0.mdl "gmm-sum-accs-2D - $dir/0.*.acc|"\
	$dir/1.mdl 2> $dir/log/update.0.log || exit 1;
  rm $dir/0.*.acc
fi


#beam=6 # will change to 10 below after 1st pass
# note: using slightly wider beams for WSJ vs. RM.
x=1
while [ $x -lt $num_iters ]; do
  echo "$0: Pass $x"
  if [ $stage -le $x ]; then
    if echo $realign_iters | grep -w $x >/dev/null; then
      echo "$0: Aligning data"
	  # gmm-boost-silence 的作用是让某些phones（由第一个参数指定）对应pdf的weight乘以--boost 参数所指定的数字，
	  # 强行提高（如果大于1）/降低（如果小于1）这个phone的概率。
      # 如果多个phone共享同一个pdf,程序中会自动做去重，乘法操作只会执行一次。
      # ORIGIN # mdl="gmm-boost-silence --boost=$boost_silence `cat $lang/phones/optional_silence.csl` $dir/$x.mdl - |"
	  mdl="$x.mdl"
	  # 执行force-alignment操作。
      # --self-loop-scale 和 --transition-scale 选项跟HMM 状态跳转相关，前者是设置自转因子，后者是非自传因子，可以修改这两个选项控制HMM的跳转倾向。
      # --acoustic-scale 选项跟GMM输出概率相关，用于平衡 GMM 输出概率和 HMM 跳转概率的重要性。
      # --beam 选项用于计算对解码过程中出现较低log-likelihood的token进行裁剪的阈值，该值设计的越小，大部分token会被裁剪以便提高解码速度，
	  #   但可能会在开始阶段把正确的token裁剪掉导致无法得到正确的解码路径。
      # --retry-beam 选项用于修正上述的问题，当无法得到正确的解码路径后，会增加beam的值，如果找到了最佳解码路径则退出，
	  #   否则一直增加指定该选项设置的值，如果还没找到，就抛出警告，导致这种问题要么是标注本来就不对，或者retry-beam也设计得太小。
      $cmd JOB=1:$nj $dir/log/align.$x.JOB.log \
	  # ORIGIN # gmm-align-compiled $scale_opts --beam=$beam --retry-beam=$[$beam*4] --careful=$careful "$mdl" \
	  # ORIGIN # "ark:gunzip -c $dir/fsts.JOB.gz|" "$feats" "ark,t:|gzip -c >$dir/ali.JOB.gz" \
        gmm-align-2D $scale_opts --careful=$careful "$mdl" "$feats" ark:$sdata/JOB/block\
        "ark:sym2int.pl --map-oov $oov_sym -f 2- $lang/words.txt < $sdata/JOB/text|" \
		"ark,t:|gzip -c >$dir/ali.JOB.gz"\
        || exit 1;
    fi
	# 更新模型 
    $cmd JOB=1:$nj $dir/log/acc.$x.JOB.log \
	# ORIGIN # gmm-acc-stats-ali-2D  $dir/$x.mdl "$feats" "ark:gunzip -c $dir/ali.JOB.gz|" \
	# ORIGIN # $dir/$x.JOB.acc || exit 1;
      gmm-acc-stats-ali-2D  $dir/$x.mdl "$feats" \
	  "ark:gunzip -c $dir/ali.JOB.gz|" \
      $dir/$x.JOB.acc || exit 1;

    $cmd $dir/log/update.$x.log \
      gmm-est-2D --write-occs=$dir/$[$x+1].occs --mix-up=$numgauss --power=$power $dir/$x.mdl \
      "gmm-sum-accs-2D - $dir/$x.*.acc|" $dir/$[$x+1].mdl || exit 1;
    rm $dir/$x.mdl $dir/$x.*.acc $dir/$x.occs 2>/dev/null
  fi
  # 线性增加混合高斯模型的数目，直到指定数量。
  if [ $x -le $max_iter_inc ]; then
     numgauss=$[$numgauss+$incgauss];
  fi
  # 提高裁剪门限。
  #beam=10
  x=$[$x+1]
done

( cd $dir; rm final.{mdl,occs} 2>/dev/null; ln -s $x.mdl final.mdl; ln -s $x.occs final.occs )

utils/summarize_warnings.pl $dir/log

echo Done

# example of showing the alignments:
# show-alignments data/lang/phones.txt $dir/30.mdl "ark:gunzip -c $dir/ali.0.gz|" | head -4

