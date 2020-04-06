#!/usr/bin/perl
use strict;  # 启用严格模式，所有变量需要用my声明作用域

die "perl ".__FILE__." <data_path> <phone_list> <suffix> <out_file>\n".
	"function: merge all files'path in <data_path> contain lines in <phone_list> and end up with <suffix> to <out_file>\n".
	"e.g. perl ".__FILE__." ./HWDB1.1 ./sub_50/phone_list_5 .bmp sub_50.list\n" if @ARGV != 4; 

my $data_path = $ENV{'PWD'}.'/'.$ARGV[0];
my $phone_list = $ENV{'PWD'}.'/'.$ARGV[1];
my $suffix = $ARGV[2];
my $out_file = $ENV{'PWD'}.'/'.$ARGV[3];

#system("cd $data_path");
#system("ls | egrep $phone_list > $out_file");
#system("sed -i 's/^/$data_path/g'");

open (PHONE, "<$phone_list") or die "can not open $phone_list ! \n";
open (FILE_PATH , ">$out_file") or die "can not open or create $out_file ! \n";

if ($data_path =~ m/\/$/){
	$data_path = $`;
}
opendir (DIR , $data_path) or die "can not open $data_path ! \n";
my @dir = readdir DIR;
my $sample_num;

while(<PHONE>) {
	my $phone = $_;
	$phone =~ m/\w+/;
	$phone = $&;
	$sample_num = 0;
	foreach my $file (@dir){
		if ($file =~ m/$phone/ and $file =~ m/$suffix$/){
			$sample_num++;
			print FILE_PATH "$data_path".'/'."$file"."\n";
		}
	}
	print " $phone has $sample_num samples.\n"
}
close (PHONE);
close (FILE_PATH);
