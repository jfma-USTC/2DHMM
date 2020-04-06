#!/usr/bin/perl
use strict;  # 启用严格模式，所有变量需要用my声明作用域

die "perl ".__FILE__." <input_path> <out_file>\n".
	"function: find all file whose 'wc -l'=0 in <input_path> and merge their paths to <out_file>\n".
	"e.g. perl ".__FILE__." ./feature_htk invalid_htk_file\n" if @ARGV != 2; 

my $input_path = $ARGV[0];
my $out_file = $ENV{'PWD'}.'/'.$ARGV[1];

opendir (DIR , $input_path) or die "can not open $input_path ! \n";
my @dir = readdir DIR;
open (FILE_PATH , ">$out_file") or die "can not open or create $out_file";

chdir ( $input_path ) or die "can not change directory to $input_path";
foreach my $file (@dir){
	my $size = -s $file;
	#print "$file = $size byte\n";
	if ($size <= 12){
		print "$file is $size byte \n";
		print FILE_PATH "$input_path".'/'."$file\n";
	}
}
print "All files in [$input_path] whose size <= '12Bytes' were printed in screen \n".
	"Which means the program didn't extract features correctly form their sourse pictures \n".
	"These invalid files' paths were written to [$out_file] allready \n";
close (FILE_PATH);
