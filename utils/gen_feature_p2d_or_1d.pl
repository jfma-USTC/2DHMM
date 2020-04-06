#!/usr/bin/perl
use strict;  # 启用严格模式，所有变量需要用my声明作用域
use Encode;  # 编译时引入Encode模块，用于编码处理

die "perl *.pl <in-file-list> <output-dir> <bin-dir> <fea-dim after-pca> [<shift_right==5> <shift_down==5>]\n".
	"usage: <in-file-list>         --file_list, each line store one data path\n".
	"       <output-dir>           --where all temp-data will be saved\n".
	"       <bin-dir>              --where binary files are saved, include { pca_main_split/easytraining/... }\n".
	"       <fea-dim after-pca>    --the fea-dimension after PCA-op, it's the dimension of features to be trained/decoded \n".
	"       <shift-right>          --optional, step size scan rightward, default 5\n".
	"       <shift-down>           --optional, step size scan downward, default 5\n".
	"                                set -1 means only scan rightward (1D-mod)\n".
	"e.g. perl ".__FILE__." train_file_list.txt train_feature fea/bin 50 5 -1\n" if not (@ARGV == 4 or @ARGV == 5 or @ARGV == 6);

my $exp_dir       = $ENV{'PWD'};
my $file_list     = $ENV{'PWD'}.'/'.$ARGV[0];
my $output_dir    = $ENV{'PWD'}.'/'.$ARGV[1];
my $BIN_DIR       = $ENV{'PWD'}.'/'.$ARGV[2];
my $dim_after_pca = $ARGV[3];
# number of parallel jobs to run
my $num_split = 20;

if ($output_dir =~ m/\/$/){
	$output_dir = $`;
}

if ($dim_after_pca <= 10 || $dim_after_pca >= 100){
	die __FILE__.": feature dimension should between [10,100], but we got $dim_after_pca\n"
}

#指定中间结果存储目录
my $split_dir       = $output_dir.'/'."split";
my $raw_htk_dir     = $output_dir.'/'."feature_htk";
my $pca_htk_path    = $output_dir.'/'."pca_htk";
my $pca_MVN_htk_dir = $output_dir.'/'."pca".$dim_after_pca."_MVN";
my $pca_matrix      = $output_dir.'/'."pca.dat";
my $mean_file       = $output_dir.'/'."feature_".$dim_after_pca.".mean";
my $var_file        = $output_dir.'/'."feature_".$dim_after_pca.".var";
my $shift_right     = 5;
my $shift_down      = 5;
my $raw_feat_dim    = 128; # defalut in P-2D situation, feats dim are 128.

if(@ARGV == 5) {
	$shift_right = $ARGV[4];
} elsif (@ARGV == 6) {
	$shift_right = $ARGV[4];
	$shift_down  = $ARGV[5];
}

if($shift_down == -1) { # if scan in 1D situation, feats dim are 256.
	$raw_feat_dim = 256;
}

print "frame parameters: shift_right = $shift_right , shift_down = $shift_down\n";

#清空或创建相应目录
if(-e $output_dir) {
	die "$output_dir already exist!";
} else {
	mkdir $output_dir;
}
if(-e $split_dir) {
	print "deleting existed split** in $split_dir ...\n";
	unlink <$split_dir/split*>; ####删除文件
	print "delete old split* files done\n";
} else {
	mkdir $split_dir;	
}
if(-e $raw_htk_dir) {
	print "deleting existed *.htk in $raw_htk_dir ...\n";
	unlink <$raw_htk_dir/*.htk>; ####删除文件
	print "delete old htk files done\n";
} else {
	mkdir $raw_htk_dir;	
}
if(-e $pca_htk_path) {
	print "deleting existed *.htk in $pca_htk_path ...\n";
	unlink <$pca_htk_path/*.htk>; ####删除文件
	print "delete old htk files done\n";
} else {
	mkdir $pca_htk_path;
}
if(-e $pca_MVN_htk_dir) {
	print "deleting existed *.htk in $pca_MVN_htk_dir ...\n";
	unlink <$pca_MVN_htk_dir/*.htk>; ####删除文件
	print "delete old htk files done\n";
} else {
	mkdir $pca_MVN_htk_dir;
}

#1、调用train_feature_extract进行特征提取，256维的灰度特征和4维的矩特征分开存储
(-e $BIN_DIR) or die "binary-files dir not exist in $BIN_DIR ! program exit.\n";
my $easytraining = "$BIN_DIR/easytraining";

my @pids;
my $scp;

# 调用shell系统命令split来将all_casia_list.txt按每个子文件197133行切分，子文件前缀为split
# -d参数表示以数字作为文件后缀，-a 2参数表示后缀长度为2
# 两个一起使用表示后缀为xxx00\xxx01\xxx02的格式
print "splitting input file list to $num_split parallel jobs ...\n";
my $file_list_rows = 0;
open TFILE, "$file_list";
while(<TFILE>){
	$file_list_rows++;
}
close (TFILE);

my $n = $file_list_rows % $num_split;
my $t = int($file_list_rows / $num_split);
if (($n + $t) <= ($num_split -1)){
        print "\ninput file has too few lines : $file_list_rows , \n there maybe some error log, just ignore them, the result is right\n\n";
}
if ($n==0){
        $file_list_rows = $t;
} else {
        $file_list_rows = $t + 1;
}

print "switch working dir to $split_dir ... \n";
chdir ( $split_dir ) or die "can not change directory to $split_dir";
print "currently working on $split_dir \n";
system("split -d -a 2  -l $file_list_rows $file_list split");
print "split done\n";
print "switch working dir to $output_dir ... \n";
chdir ( $output_dir ) or die "can not change directory to $output_dir";
print "currently working on $output_dir \n";

print "extracting features ...\n";
for(my $i=0;$i<$num_split;$i++)
{
	my $pid = fork();
	if($pid==0) {	
		if($i < 10) {
			$scp = $split_dir.'/'."split0$i";
		} else{
			$scp = $split_dir.'/'."split$i";
		}
		system("$BIN_DIR/extract_8_direction_pseudo $scp $raw_htk_dir $shift_right $shift_down 1");
		exit(0);
	}
	push @pids,$pid;
}

foreach(@pids){
	waitpid($_,0);
}

my @file_name_arr;
my $index = 0;
opendir (DIR , $raw_htk_dir) or die "can not open $raw_htk_dir ! \n";
my @dir = readdir DIR;
foreach my $file (@dir){
	if(!($file eq "." or $file eq "..")){
		$file_name_arr[$index++] = $file; # 将文件名xxx.htk存入file_name_arr数组
	}
}

###创建中间结果列表文件
open(GET_PCA_LIST, ">$output_dir/get_pca_list.scp") || die;
open(APPLY_PCA_LIST, ">$output_dir/apply_pca_list.scp") || die;
open(GET_MEAN_VAR_LIST, ">$output_dir/get_mean_var_list.scp") || die;
open(PCA_MOMENT_MVN_LIST, ">$output_dir/pca_MVN_list.scp") || die;

for(my $i = 0;$i <= $#file_name_arr;$i++) {
	my $raw_htk = "$raw_htk_dir/$file_name_arr[$i]";
	my $pca = "$pca_htk_path/$file_name_arr[$i]";
	my $pca_MVN_htk_dir = "$pca_MVN_htk_dir/$file_name_arr[$i]";
	
	print GET_PCA_LIST "$raw_htk\n";
	print APPLY_PCA_LIST "$raw_htk   $pca\n"; ###原特征地址和现在生成的pca特征地址
	print GET_MEAN_VAR_LIST "$pca\n";       ###pca特征的地址,产生均值和方差
	print PCA_MOMENT_MVN_LIST "$pca   $pca_MVN_htk_dir\n";###对pca特征做mvn
}

close(GET_PCA_LIST);
close(APPLY_PCA_LIST);
close(GET_MEAN_VAR_LIST);
close(PCA_MOMENT_MVN_LIST);

###2、根据1中的256维灰度特征计算PCA矩阵，并保存到输出目录下，矩阵维度为256*$dim_after_pca

my $pca_part_data = $output_dir.'/'."PCA_part_data";
if(-e $pca_part_data) {
	unlink <$pca_part_data/*.txt>;
} else {
	mkdir $pca_part_data;	
}
my $get_pca_list = $output_dir.'/'."get_pca_list.scp"; # get_pca_list.scp指向的是feture_htk文件夹下的.htk文件
print("$easytraining  -SplitScript $get_pca_list $num_split\n");
system("$easytraining -SplitScript $get_pca_list $num_split"); #任务分解成20份

my @pids2; # 并行执行，将分解后的子任务$scp进行pca降维并存放在PCA_part_data文件夹下
for(my $i=1;$i<=$num_split;$i++)
{
	 my $pid = fork();
	 if($pid==0)
	 {
		 my $scp = "$get_pca_list.$i";
		 my $part_pca = "$pca_part_data/part.$i\.txt";
		
		 system("$BIN_DIR/pca_main_split $scp $part_pca $raw_feat_dim $dim_after_pca 0");
		 exit(0);
	 }
	 push @pids2,$pid;
}

foreach(@pids2){
	waitpid($_,0);
}
for(my $i=1;$i<=$num_split;$i++)
{
	unlink("$get_pca_list.$i");
}

my $part_pca_lst = $output_dir.'/'."part_pca_list.scp"; #合并降维后的20个文件所在路径
system("ls $pca_part_data/*.txt > $part_pca_lst");

system("$BIN_DIR/pca_main_split $part_pca_lst $pca_matrix $raw_feat_dim $dim_after_pca 1");

##############生成了pca矩阵###########


#3、对1中得到的256维灰度特征执行pca降维

my $apply_pca_list = $output_dir.'/'."apply_pca_list.scp";
print("$easytraining  -SplitScript $apply_pca_list $num_split\n");
system("$easytraining -SplitScript $apply_pca_list $num_split");
my @pids2;
for(my $i=1;$i<=$num_split;$i++)
{
	 my $pid = fork();
	 if($pid==0)
	 {
		 my $scp = "$apply_pca_list.$i";

		 system("$BIN_DIR/pca_main_split $scp $pca_matrix $raw_feat_dim $dim_after_pca 2");
		 exit(0);
	 }
	 push @pids2,$pid;

}

foreach(@pids2){
	waitpid($_,0);
}
for(my $i=1;$i<=$num_split;$i++)
{
	unlink("$apply_pca_list.$i");
}


#########产生了pca特征################



###5、计算训练集的均值和方差
my $mean_var_dir = $output_dir.'/'."mean_var_dir";
if(-e $mean_var_dir)
{
	unlink <$mean_var_dir/*.txt>;
}
else
{
	mkdir $mean_var_dir;	
}

my $get_mean_var_list = $output_dir.'/'."get_mean_var_list.scp";

print("$easytraining  -SplitScript $get_mean_var_list $num_split\n");
system("$easytraining -SplitScript $get_mean_var_list $num_split");
my @pids2;
for(my $i=1;$i<=$num_split;$i++)
{
	 my $pid = fork();
	 if($pid==0)
	 {
		 my $scp = "$get_mean_var_list.$i";
		 my $mean = "$mean_var_dir/mean_part.$i\.txt";
		 my $var  = "$mean_var_dir/var_part.$i\.txt";

		 system("$BIN_DIR/calcMeanVar_split.bin $scp $dim_after_pca $mean $var");
		 exit(0);
	 }
	 push @pids2,$pid;
}

foreach(@pids2){
	waitpid($_,0);
}
for(my $i=1;$i<=$num_split;$i++)
{
	unlink("$get_mean_var_list.$i");
}

my $mean_list = $output_dir.'/'."mean_file_list.scp";
my $var_list = $output_dir.'/'."var_file_list.scp";
system("ls $mean_var_dir/mean*.txt > $mean_list");
system("ls $mean_var_dir/var*.txt > $var_list");
system("$BIN_DIR/mean_var_union $mean_list $var_list $mean_file $var_file $dim_after_pca");

#6、对5中得到的特征执行MVN：减均值除标准差
my $apply_mvn_list = $output_dir.'/'."pca_MVN_list.scp";
print("$easytraining  -SplitScript $apply_mvn_list $num_split\n");
system("$easytraining -SplitScript $apply_mvn_list $num_split");
my @pids3;
for(my $i=1;$i<=$num_split;$i++)
{
	 my $pid = fork();
	 if($pid==0)
	 {
		 my $scp = "$apply_mvn_list.$i";

		 system("$BIN_DIR/MVN $scp $mean_file $var_file $dim_after_pca");
		 exit(0);
	 }
	 push @pids3,$pid;

}

foreach(@pids3){
	waitpid($_,0);
}
for(my $i=1;$i<=$num_split;$i++)
{
	unlink("$apply_mvn_list.$i");
}

print "switch working dir to $exp_dir ... \n";
chdir( $exp_dir ) or die "cannot change dir to $exp_dir \n";
print "currently working on $exp_dir \n";
