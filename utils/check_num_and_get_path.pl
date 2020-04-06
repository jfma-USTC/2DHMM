#!/usr/bin/perl
use strict;  # 启用严格模式，所有变量需要用my声明作用域

die "perl ".__FILE__." <input_path> <input_file> <suffix> <out_file>\n".
	"function: find all files whose name appeared in <input_file>\n".
	"and ended with <suffix> in <input_path> and write their paths to <out_file>\n".
	"e.g. perl ".__FILE__." ./data/HWDB1.1 sub_200.txt .bmp sub_200_bmp_path.txt\n" if @ARGV != 4; 

my $input_path = $ARGV[0];
my $input_file = $ARGV[1];
my $suffix = $ARGV[2];
my $out_file = $ENV{'PWD'}.'/'.$ARGV[3];

opendir(DIR , $input_path) or die "can not open $input_path ! \n";
my @dir = readdir DIR;
open(LINES_IN , "<$input_file") or die "can't open $input_file !\n";
open (FILE_PATH , ">$out_file") or die "can not open or create $out_file";
while(<LINES_IN>){
	$_ =~ /.+/;
	my $pure_line = $&;
	my $count = 0;
	foreach my $file (@dir){
		if (($file =~ m/$suffix/) and ($file =~ m/$pure_line/)){
			print FILE_PATH "$input_path".'/'."$file"."\n";
			$count = $count +1;
		}
	}
	print "$pure_line : $count files in $input_file\n";
}
close (FILE_PATH);
