#!/usr/bin/perl
use strict;  # 启用严格模式，所有变量需要用my声明作用域

die "perl ".__FILE__." <file_list> <lexcion> <out_file>\n".
	"function: use <file_list> and <lexcion> to get <out_file>(text) for each instance in file_list\n".
	"e.g. perl ".__FILE__." file_list lexcion.txt text\n" if @ARGV != 3; 

my $file_list = $ARGV[0];
my $lexcion = $ARGV[1];
my $text = $ARGV[2];

open (FILE_NAME , "<$file_list") or die "can not open $file_list";
open (LEXCION , "<$lexcion") or die "can not open $lexcion";
open (TEXT , ">$text") or die "can not open or create $text";

my %lex_pair;

while(<LEXCION>){
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

while(<FILE_NAME>){
	$_ =~ /\./;
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

close (FILE_NAME);
close (LEXCION);
close (TEXT);
