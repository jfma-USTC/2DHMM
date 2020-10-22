#!/usr/bin/perl
use strict;  # 启用严格模式，所有变量需要用my声明作用域
use Encode;  # 编译时引入Encode模块，用于编码处理
use File::Path;  # https://perldoc.perl.org/File/Path.html
use File::Basename;  # 可以分割文件路径、文件名称、文件后缀的库

die "perl *.pl <in-file-list> <tmp-htk-list>\n".
	"usage: extract fetures of files listed in <in-file-list> in htk-format \n".
	"<tmp-htk-list> is a file that list all .htk file path in ./feature_htk \n".
	"e.g. perl ".__FILE__." train_file_list.txt hwdb1_1_list_htk_128.txt\n" if @ARGV != 1;
	# PERL *PL  train_file_list.txt out_path

my $file_list  = $ARGV[0];
my $htk_list  = $ARGV[1];
my $pca_matrix = "./pca.dat";
my $mean_file  = "./feature_50.mean";
my $var_file   = "./feature_50.var";

#指定中间结果存储目录
my $out_dir= "feature_htk";
my $pca_path = "pca_htk";
my $pca_MVN= "pca50_MVN";

#清空或创建相应目录
if(-e $out_dir) {
	print "deleting existed *.htk ...\n";
	unlink <$out_dir/*.htk>; ####删除文件
	print "delete htk files done\n";
} else {
	mkdir $out_dir;	
}
if(-e $pca_path) {
	unlink <$pca_path/*.htk>;
} else {
	mkdir $pca_path;
}
if(-e $pca_MVN) {
	unlink <$pca_MVN/*.htk>;
} else {
	mkdir $pca_MVN;
}

#1、调用train_feature_extract进行特征提取，256维的灰度特征和4维的矩特征分开存储
my $BIN_DIR = "./bin";
my $easytraining = "$BIN_DIR/easytraining";
my $num_split = 20;
my @pids;
my $scp;

# 调用shell系统命令split来将all_casia_list.txt按每个子文件197133行切分，子文件前缀为split
# -d参数表示以数字作为文件后缀，-a 2参数表示后缀长度为2
# 两个一起使用表示后缀为xxx00\xxx01\xxx02的格式
print "splitting input file list to 20 parallel jobs ...\n";
system("split -d -a 2  -l 58630 $file_list split");
print "split done\n";

print "extracting features ...";
for(my $i=0;$i<=$num_split;$i++)
{
	 my $pid = fork();
	 if($pid==0) {	
		 if($i < 10) {
			$scp = "split0$i";
		 } else{
			$scp = "split$i";
         }
		 system("$BIN_DIR/extract_8_direction -L $scp -O $out_dir");
		 exit(0);
	 }
	 push @pids,$pid;
}

foreach(@pids){
	waitpid($_,0);
}

#for(my $i=1;$i<=$num_split;$i++)
#{
#	unlink("$file_list.$i");
#}

#my $htk_list = "./hwdb1_1_list_htk_128.txt";
#system("find $out_dir -name \"*.htk\" > $htk_list");
open(LIST, $htk_list) || die;

my @file_name_arr;
my $index = 0;

while(my $file_name = <LIST>)
{
	chomp $file_name; # 删除结尾的换行符\n
	
	my @field = split(/\//, $file_name); # 按/分割文件
	my $file = $field[$#field]; # 选取最后一个分割字串，即文件名
	my @fname = split(/\./, $file);
	my $name = "$fname[0].htk";
	$file_name_arr[$index++] = $name; # 将文件名xxx.htk存入file_name_arr数组
}
close(LIST);

###创建中间结果列表文件
open(GET_PCA_LIST, ">./get_pca_list.scp") || die;
open(APPLY_PCA_LIST, ">./apply_pca_list.scp") || die;
open(GET_MEAN_VAR_LIST, ">./get_mean_var_list.scp") || die;
open(PCA_MOMENT_MVN_LIST, ">./pca_MVN_list.scp") || die;

my $currDir = dirname(__FILE__);
# 此时由于__FILE__为脚本文件名，通过dirname得到的工作路径就是本文件夹路径 = '.'

for(my $i = 0;$i <= $#file_name_arr;$i++) {
	my $pixel = "$currDir/$out_dir/$file_name_arr[$i]";  
	my $pca = "$currDir/$pca_path/$file_name_arr[$i]";
	my $pca_MVN = "$currDir/$pca_MVN/$file_name_arr[$i]";
	
	print GET_PCA_LIST "$pixel\n";
	print APPLY_PCA_LIST "$pixel   $pca\n"; ###原特征地址和现在生成的pca特征地址
	print GET_MEAN_VAR_LIST "$pca\n";       ###pca特征的地址,产生均值和方差
	print PCA_MOMENT_MVN_LIST "$pca   $pca_MVN\n";###对pca特征做mvn
}

close(GET_PCA_LIST);
close(APPLY_PCA_LIST);
close(GET_MEAN_VAR_LIST);
close(PCA_MOMENT_MVN_LIST);

###2、根据1中的256维灰度特征计算PCA矩阵，并保存到输出目录下，矩阵维度为256*50

my $pca_part_data = "./PCA_part_data";
if(-e $pca_part_data) {
	unlink <$pca_part_data/*.txt>;
} else {
	mkdir $pca_part_data;	
}
my $get_pca_list = "./get_pca_list.scp"; # get_pca_list.scp指向的是feture_htk文件夹下的.htk文件
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
		
		 system("$BIN_DIR/pca_main_split $scp $part_pca 256 50 0");
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

my $part_pca_lst = "./part_pca_list.scp"; #合并降维后的20个文件所在路径
system("ls $pca_part_data/*.txt > $part_pca_lst");

system("$BIN_DIR/pca_main_split $part_pca_lst $pca_matrix 256 50 1");

##############生成了pca矩阵###########


#3、对1中得到的256维灰度特征执行pca降维

my $apply_pca_list = "./apply_pca_list.scp";
print("$easytraining  -SplitScript $apply_pca_list $num_split\n");
system("$easytraining -SplitScript $apply_pca_list $num_split");
my @pids2;
for(my $i=1;$i<=$num_split;$i++)
{
	 my $pid = fork();
	 if($pid==0)
	 {
		 my $scp = "$apply_pca_list.$i";

		 system("$BIN_DIR/pca_main_split $scp $pca_matrix 256 50 2");
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
my $mean_var_dir = "./mean_var_dir";
if(-e $mean_var_dir)
{
	unlink <$mean_var_dir/*.txt>;
}
else
{
	mkdir $mean_var_dir;	
}

my $get_mean_var_list = "./get_mean_var_list.scp";

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

		 system("$BIN_DIR/calcMeanVar_split.bin $scp 50 $mean $var");
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

my $mean_list = "./mean_file_list.scp";
my $var_list = "./var_file_list.scp";
system("ls $mean_var_dir/mean*.txt > $mean_list");
system("ls $mean_var_dir/var*.txt > $var_list");
system("$BIN_DIR/mean_var_union $mean_list $var_list $mean_file $var_file 50");


#6、对5中得到的特征执行MVN：减均值除标准差
my $apply_mvn_list = "./pca_MVN_list.scp";
print("$easytraining  -SplitScript $apply_mvn_list $num_split\n");
system("$easytraining -SplitScript $apply_mvn_list $num_split");
my @pids3;
for(my $i=1;$i<=$num_split;$i++)
{
	 my $pid = fork();
	 if($pid==0)
	 {
		 my $scp = "$apply_mvn_list.$i";

		 system("$BIN_DIR/MVN $scp $mean_file $var_file 50");
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
