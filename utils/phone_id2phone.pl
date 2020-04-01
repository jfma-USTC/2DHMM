#!/usr/bin/perl
use strict;  # 启用严格模式，所有变量需要用my声明作用域

die "perl ".__FILE__." <phones> <likely_phones_id_seq_dir> <output_sum_phones_seq>\n".
	"function: map all phones_id_seq_dir in <likely_phones_id_seq_dir> to <output_sum_phones_seq>\n".
	"e.g. perl ".__FILE__." phones.txt likely_phones_dir most_likely_phones.txt\n" if  @ARGV != 3; 

my $phones  = $ARGV[0];
my $phones_id_seq_dir   = $ARGV[1];
my $sum_phones_name_seq = $ENV{'PWD'}.'/'.$ARGV[2];
my $suffix = ".phones_id";

open (PHONE, "<$phones") or die "can not open $phones";
open (PHONE_NAME_SEQ, ">$sum_phones_name_seq") or die "can not open $sum_phones_name_seq";

my %phone_pair;

while(<PHONE>){
	$_ =~ m/ /;
	my $phone = $`;
	my $phone_id = $';
	chomp($phone_id);
	$phone_pair{$phone_id} = $phone;
	#print "phone_id: $phone_id -> phone: $phone\n";
}

if ($phones_id_seq_dir =~ m/\/$/){
	$phones_id_seq_dir = $`;
}

opendir (DIR , $phones_id_seq_dir) or die "can not open $phones_id_seq_dir ! \n";
my @files = readdir DIR;
my $sample_num = 0;
my $file_num = 0;

foreach my $file (@files){
	next unless $file =~ m/$suffix$/;
	$file_num++;
	
	$file = "$phones_id_seq_dir/$file";
	open (PHONE_ID, "<$file") or die "can not open $file";
	while(<PHONE_ID>) {
		$sample_num++;
		my @A = split(" ", $_);
		@A != 1 || die "bad line in $file: $_";
		my $sample_name = $A[0];
		print PHONE_NAME_SEQ "$sample_name ";
		shift @A;
		foreach my $phone_id (@A){
			print PHONE_NAME_SEQ "$phone_pair{$phone_id} ";
		}
		print PHONE_NAME_SEQ "\n";
	}
	close (PHONE_ID);
}

print PHONE_NAME_SEQ "All $file_num files done, translated $sample_num samples.\n";

close (PHONE);
close (PHONE_NAME_SEQ);
closedir DIR;
