#!/usr/bin/perl
#use strict;  # 启用严格模式，所有变量需要用my声明作用域

die "perl ".__FILE__." <in_file> <ref_file> [option]\n".
	"function: check whether lines in <in_file> are included in <ref_file>\n".
	"e.g. perl ".__FILE__." sub_set.txt tol_set.txt\n".
	"Option:\n".
	"--delete       delete lines in <in_file> that is not included in <ref_file>.\n" if @ARGV != 2 and @ARGV != 3; 

my $del = 0;
my $in_file = $ARGV[0];
my $ref_file = $ARGV[1];

if ( @ARGV == 3 ) {
	if ( $ARGV[2] eq "--delete" ){
		$del = 1;
	}
	else{
		die "option invalid, use '--delete' instead \n";
	}
}

open(LINES_IN , "<$in_file") or die "can't open $in_file !\n";
open(LINES_REF , "<$ref_file") or die "can't open $ref_file !\n"; 

my @valid_line;
my @invalid_line;
my @ref_lines;

while(<LINES_REF>){
	$_ =~ /.+/;
	#print $&;
	push @ref_lines,$&;
}

while(<LINES_IN>){
	my $is_match = 0;
	$_ =~ /.+/;
	my $pure_line = $&;
	my $match = grep { $_ eq $pure_line } @ref_lines; 
	#print "$match ";
	if( $match ){
		$is_match = 1;
	}
	if($is_match){
		push @valid_line,$pure_line;
	}
	else{
		push @invalid_line,$pure_line;
		print "line $pure_line not included!\n";
	}
}

if($invalid_line == 0){
	my $line_num = $valid_line;
	print "total $line_num line is included!\n";
}

#foreach (@invalid_line) {
#	print "$_ ";
#}

close(LINES_IN);
close(LINES_REF);

#print "del=$del\n";

if($del){
	open(LINES_IN , ">$in_file") or die "can't open $in_file to write in !\n";
	foreach my $line (@valid_line){
		print LINES_IN "$line\n";
	}
	close(LINES_IN);
}

