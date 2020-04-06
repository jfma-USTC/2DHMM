#!/usr/bin/perl
use strict;  # 启用严格模式，所有变量需要用my声明作用域

die "perl ".__FILE__." <input_path> <suffix> <out_file>\n".
	"function: merge all files'path in <input_path> end with <suffix> to <out_file>\n".
	"e.g. perl ".__FILE__." ./HWDB1.1 .bmp all_casia_list\n" if @ARGV != 3; 

my $input_path = $ENV{'PWD'}.'/'.$ARGV[0];
my $suffix = $ARGV[1];
my $out_file = $ENV{'PWD'}.'/'.$ARGV[2];

if ($input_path =~ m/\/$/){
	$input_path = $`;
}
opendir (DIR , $input_path) or die "can not open $input_path ! \n";
my @dir = readdir DIR;
open (FILE_PATH , ">$out_file") or die "can not open or create $out_file";
foreach my $file (@dir){
	if ($file =~ m/$suffix/){
		print FILE_PATH "$input_path".'/'."$file"."\n";
	}
}
close (FILE_PATH);
