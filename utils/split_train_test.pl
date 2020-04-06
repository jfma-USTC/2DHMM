#!/usr/bin/perl
use strict;  # 启用严格模式，所有变量需要用my声明作用域
use List::Util;

die "perl ".__FILE__." <file_list> <phone_list> <train_size/test_size> <train_list> <test_list>\n".
	"function: split <file_list> as ratio of <train_size/test_size> to <train_list> & <test_list>\n".
	"e.g. perl ".__FILE__." sub_50.list phone_list_50 6 train_50 test_50\n" if @ARGV != 5; 

my $file_list  = $ARGV[0];
my $phone_list = $ARGV[1];
my $ratio      = $ARGV[2];
my $train_list = $ARGV[3];
my $test_list  = $ARGV[4];

open (SAMPLE, "<$file_list")  or die "can not open $file_list ! \n";
open (PHONE,  "<$phone_list") or die "can not open $phone_list ! \n";
open (TRAIN,  ">$train_list") or die "can not create $train_list ! \n";
open (TEST,   ">$test_list")  or die "can not create $test_list ! \n";

my @all_data;
my @sample;
my $sample_num;
my $train_num;
my $test_num;

while(<SAMPLE>){
	push @all_data, $_;
}

while(<PHONE>) {
	my $phone = $_;
	$phone =~ m/\w+/;
	$phone = $&;
	$sample_num = 0;
	foreach my $file (@all_data){
		if ($file =~ m/$phone/){
			$sample_num++;
			push @sample, $file;
		}
	}
	$test_num = int($sample_num / ($ratio + 1));
	$train_num = $sample_num - $test_num;
	
	@sample=List::Util::shuffle @sample;
	
	my $index;
	for ($index = 0; $index < $train_num; $index++){
		print TRAIN $sample[$index];
	}
	for(; $index <= $#sample; $index++){
		print TEST $sample[$index];
	}
	print " $phone has $sample_num samples, $train_num train samples, $test_num test samples\n";
	@sample = ();
}
close (PHONE);
close (SAMPLE);
close (TRAIN);
close (TEST);