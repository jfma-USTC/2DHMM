#!/usr/bin/perl
use strict;  # 启用严格模式，所有变量需要用my声明作用域

die "perl ".__FILE__." <lexicon> <phones> <input_text> > output_transcriptions\n".
	"function: use <lexicon> and <phones> to map <input_text> to output_transcriptions\n".
	"e.g. perl ".__FILE__." lexicon.txt phones.txt text > trans\n" if  @ARGV != 3; 

my $lexicon = $ARGV[0];
my $phones  = $ARGV[1];
my $text    = $ARGV[2];

open (LEXCION , "<$lexicon") or die "can not open $lexicon";
open (PHONE ,   "<$phones")  or die "can not open $phones";
open (TEXT ,    "<$text")    or die "can not open $text";

my %lex_pair;
my %phone_pair;

while(<LEXCION>){
	$_ =~ m/^[^ ]+ /;
	my $word = $&;
	my $phone_list = $';
	chomp($phone_list);
	$word =~ m/ /;
	$word = $`;
	$lex_pair{$word} = $phone_list;
	#print "word: $word -> phone_list: $phone_list\n";
}

while(<PHONE>){
	$_ =~ m/ /;
	my $phone = $`;
	my $phone_id = $';
	chomp($phone_id);
	$phone_pair{$phone} = $phone_id;
	#print "phone: $phone -> phone_id: $phone_id\n";
}

while(<TEXT>){
	my @text_contain = split(" ",$_);
	my $sample_name = $text_contain[0];
	my @phone_ids;
	push @phone_ids, $sample_name;
	for (my $i=1; $i < @text_contain; $i++){
		my $word = $text_contain[$i];
		my $phone_list = $lex_pair{$word};
		if (defined $phone_list){
			my @phones = split(" ",$phone_list);
			for (my $j=0; $j < @phones; $j++){
				my $phone = $phones[$j];
				my $phone_id = $phone_pair{$phone};
				if (defined $phone_id){
					push @phone_ids, $phone_id;
				} else {
					die __FILE__." undefined phone $phone\n";
				}
			}
		} else {
			die __FILE__." undefined word $word (in position $i)\n";
		}
	}
	print join(" ", @phone_ids);
	print "\n";
}

close (LEXCION);
close (PHONE);
close (TEXT);
