#!/usr/bin/perl
use strict;  # 启用严格模式，所有变量需要用my声明作用域

die "perl ".__FILE__." <wer_dir> <sum_wer>\n".
	"function: read all wer file in <wer_dir> and calculate overall WER to <sum_wer>\n".
	"e.g. perl ".__FILE__." wer_log tol_wer.txt\n" if  @ARGV != 2; 

my $wer_dir = $ARGV[0];
my $tol_wer = $ARGV[1];
my $suffix = ".wer";

open (TOL_WER, ">$tol_wer") or die "can not open $tol_wer";

if ($wer_dir =~ m/\/$/){
	$wer_dir = $`;
}

opendir (DIR , $wer_dir) or die "can not open $wer_dir ! \n";
my @wer_files = readdir DIR;
my $file_num = 0;
my $sample_num = 0;
my $error_num = 0;
my $top1_right_num = 0;
my $top5_right_num = 0;
my $top10_right_num = 0;
my $top_num1_right_num = 0;
my $bool_have_top_num1 = 0;
my $top_num1_name;

foreach my $wer_file (@wer_files){
	next unless $wer_file =~ m/$suffix$/;
	$file_num++;
	$wer_file = "$wer_dir/$wer_file";
	open (WER_INFO, "<$wer_file") or die "can not open $wer_file";
	while(<WER_INFO>) {
		my @A = split(" ", $_);
		@A != 1 || die "bad line in $wer_file: $_";
		if ($A[0] =~ m/Done/){
			$sample_num += ($A[$#A] + 0);
		}
		elsif ($A[0] =~ m/Error/){
			$error_num += ($A[$#A] + 0);
		}
		elsif ($A[0] =~ m/right/){
			if ($A[0] =~ m/Top1_/){
				$top1_right_num += ($A[$#A] + 0);
			}
			elsif ($A[0] =~ m/Top5_/){
				$top5_right_num += ($A[$#A] + 0);
			}
			elsif ($A[0] =~ m/Top10_/){
				$top10_right_num += ($A[$#A] + 0);
			}
			else {
				$top_num1_name = $A[0];
				$bool_have_top_num1 = 1;
				$top_num1_right_num += ($A[$#A] + 0);
			}
		}
	}
	close (WER_INFO);
}

my $top1_wer = 1 - $top1_right_num / $sample_num;
my $top5_wer = 1 - $top5_right_num / $sample_num;
my $top10_wer = 1 - $top10_right_num / $sample_num;
my $top_num1_wer = 1 - $top_num1_right_num / $sample_num;

print TOL_WER "All $file_num files done, calculating over $sample_num samples, $error_num samples have error.\n";
print TOL_WER "Top1_right samples: $top1_right_num\n";
print TOL_WER "Top5_right samples: $top5_right_num\n";
print TOL_WER "Top10_right samples: $top10_right_num\n";
if ($bool_have_top_num1){
	print TOL_WER "Top1_right samples: $top1_right_num\n";
}
print TOL_WER "Top1_wer: $top1_wer\n";
print TOL_WER "Top5_wer: $top5_wer\n";
print TOL_WER "Top10_wer: $top10_wer\n";
if ($bool_have_top_num1){
	$top_num1_name =~ s/right/wer/;
	print TOL_WER "$top_num1_name: $top_num1_wer\n";
}

close (TOL_WER);
closedir DIR;
