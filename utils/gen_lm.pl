#!/usr/bin/perl
use strict;  # 启用严格模式，所有变量需要用my声明作用域

die "perl ".__FILE__." <lexicon> <1-gram-lm-dir>\n".
	"function: use <lexicon> to generate <1-gram-lm-dir>\n".
	"e.g. perl ".__FILE__." lang/dict/lexicon.txt lang/ \n" if @ARGV != 2; 

my $lexicon = $ARGV[0];
my $lm = $ARGV[1];
if ($lm =~ /\/$/){
	$lm = $`;
}
open (LEXICON , "<$lexicon") or die "can not open $lexicon";

my @all_word;
while(<LEXICON>){
	if ($_ =~ m/UC/){
		my $w = $`;
		my $uc = $';
		$w =~ m/^w\d+/;
		$w = $&;
		$uc =~ m/^\w{4}/;
		$uc = $&;
		push @all_word, $w;
	}
}

my $how_many_words = @all_word;
$lm = "$lm"."/lm_"."$how_many_words"."_1gram";
open (LM , ">$lm") or die "can not create $lm";

my $tol_gram_num = $how_many_words + 4;
print LM "\n";
print LM "\\data\\\n";
print LM "ngram 1=$tol_gram_num\n\n";
print LM "\\1-grams:\n";
print LM "-0.30103  </s>\n";
print LM "-99  <s>\n";
print LM "-99  <unk>\n";
print LM "-99  sil\n";

foreach my $w (@all_word){
	print LM "-3.900586  $w\n";
}

print LM "\n\\end\\\n";

close (LEXICON);
close (LM);
