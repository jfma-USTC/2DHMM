#!/usr/bin/perl

# Copyright 2012  Johns Hopkins University (author: Daniel Povey)

# Generate a topology file.  This allows control of the number of states in the
# non-silence HMMs, and in the silence HMMs.

if(@ARGV != 3) {
  print STDERR "Usage: utils/gen_topo_2D.pl <phone_num> <state_cols> <state_rows>\n";
  print STDERR "e.g.:  utils/gen_topo_2D.pl 200 3 3\n";
  exit (1);
}

($phone_num,$cols,$rows) = @ARGV;

print "<Topology>\n";
print "<TopologyEntry>\n";
print "<ForPhones>\n";
for($i = 1; $i < $phone_num; $i++){
	print "$i ";
}
print "$phone_num\n";
print "</ForPhones>\n";

print "<TopDown>\n";
my $state = 0;
for ($row = 1; $row < $rows; $row++){
	my $col = 0;
	while ($col < $cols){
		print "<State> $state <PdfClass> $state ";
		if ($col == 0){
			print "<Transition> $state 0.5 <Transition> ${\($state+$cols)} 0.25 <Transition> ${\($state+$cols+1)} 0.25 </State>\n";
		}elsif ($col == ($cols-1)){
			print "<Transition> $state 0.4 <Transition> ${\($state+$cols-1)} 0.2 <Transition> ${\($state+$cols)} 0.2 <Transition> ${\($state+$cols+1)} 0.2 </State>\n";
		}else{
			print "<Transition> $state 0.5 <Transition> ${\($state+$cols-1)} 0.25 <Transition> ${\($state+$cols)} 0.25 </State>\n";
		}
		$col++;
		$state++;
	}
}
my $col =0;
while ($col < $cols){
	print "<State> $state <PdfClass> $state <Transition> $state 1 </State>\n";
	$col++;
	$state++;
}
print "</TopDown>\n";

print "<LeftRight>\n";
$state = 0;
$col =0;
while ($col < $cols){
	print "<State> $state <PdfClass> $state ";
		if ($col == ($cols-1)){
			print "<Transition> $state 1 </State>\n";
		}else{
			print "<Transition> $state 0.5 <Transition> ${\($state+1)} 0.25 <Transition> ${\($state+$cols+1)} 0.25 </State>\n";
		}
	$col++;
	$state++;
}
for ($row = 2; $row < $rows; $row++){
	my $col = 0;
	while ($col < $cols){
		print "<State> $state <PdfClass> $state ";
		if ($col == ($cols-1)){
			print "<Transition> $state 1 </State>\n";
		}else{
			print "<Transition> $state 0.4 <Transition> ${\($state-$cols+1)} 0.2 <Transition> ${\($state+1)} 0.2 <Transition> ${\($state+$cols+1)} 0.2 </State>\n";
		}
		$col++;
		$state++;
	}
}
$col =0;
while ($col < $cols){
	print "<State> $state <PdfClass> $state ";
		if ($col == ($cols-1)){
			print "<Transition> $state 1 </State>\n";
		}else{
			print "<Transition> $state 0.5 <Transition> ${\($state-$cols+1)} 0.25 <Transition> ${\($state+1)} 0.25 </State>\n";
		}
	$col++;
	$state++;
}
print "</LeftRight>\n";

print "</TopologyEntry>\n";
