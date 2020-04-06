#!/usr/bin/perl
use strict;  # 启用严格模式，所有变量需要用my声明作用域

die "perl ".__FILE__." <scp_file> <lexicon> <out_file>\n".
	"function: use <file_list> and <lexicon> to get <out_file>(text) for each instance in <scp_file>\n".
	"e.g. perl ".__FILE__." feats.scp lexicon.txt text\n" if @ARGV != 3; 

my $scp_file = $ARGV[0];
my $lexicon = $ARGV[1];
my $text = $ARGV[2];

open (SCP_FILE , "<$scp_file") or die "can not open $scp_file";
open (LEXICON , "<$lexicon") or die "can not open $lexicon";
open (TEXT , ">$text") or die "can not open or create $text";

my %lex_pair;

while(<LEXICON>){
	if ($_ =~ m/UC/){
		my $w = $`;
		my $uc = $';
		$w =~ m/^w\d+/;
		$w = $&;
		$uc =~ m/^\w{4}/;
		$uc = $&;
		$lex_pair{$uc} = $w;
		#print "$uc $w\n";
	}
}

while(<SCP_FILE>){
	$_ =~ / /;
	my $pure_name = $`;
	#print "$pure_name\n";
	if ($pure_name =~ /^.*\//){
		$pure_name = $';
	}
	$pure_name =~ m/_/;
	my $uc = $';
	#print "$pure_name,,,$uc\n";
	my $text = "$pure_name"." "."$lex_pair{$uc}\n";
	print TEXT "$text";
}

close (SCP_FILE);
close (LEXICON);
close (TEXT);
