#!/usr/bin/perl

# Copyright 2012  Johns Hopkins University (author: Daniel Povey)

# Generate a topology file.  This allows control of the number of states in the
# non-silence HMMs, and in the silence HMMs.

if(@ARGV != 5) {
  print STDERR "Usage: utils/gen_topo_2D.pl <topo> <sil-num> <phone-num> <super-state> <sub-state>\n";
  print STDERR "e.g.:  utils/gen_topo_2D.pl topo 2 50 3 2\n";
  exit (1);
}

$topo       = $ARGV[0];
$sil_num    = $ARGV[1];
$phone_num  = $ARGV[2];
$superstate = $ARGV[3];
$substate   = $ARGV[4];
open (TOPO , ">$topo") or die "can not open or create $topo";

$tol_num = $sil_num+$phone_num;
$state_num = $superstate*$substate;

print TOPO "<Topology> \n";
print TOPO "<TopologyEntry> \n";
print TOPO "<ForPhones> \n";
for($i = 1; $i < $sil_num; $i++){
	print TOPO "$i ";
}
print TOPO "$sil_num\n";
print TOPO "</ForPhones> \n";
print TOPO "<State> 0 <PdfClass> 0 <Transition> 0 0.25 <Transition> 1 0.25 <Transition> 2 0.25 <Transition> 3 0.25 </State> \n";
print TOPO "<State> 1 <PdfClass> 1 <Transition> 1 0.25 <Transition> 2 0.25 <Transition> 3 0.25 <Transition> 4 0.25 </State> \n";
print TOPO "<State> 2 <PdfClass> 2 <Transition> 1 0.25 <Transition> 2 0.25 <Transition> 3 0.25 <Transition> 4 0.25 </State> \n";
print TOPO "<State> 3 <PdfClass> 3 <Transition> 1 0.25 <Transition> 2 0.25 <Transition> 3 0.25 <Transition> 4 0.25 </State> \n";
print TOPO "<State> 4 <PdfClass> 4 <Transition> 4 0.75 <Transition> 5 0.25 </State> \n";
print TOPO "<State> 5 </State> \n";
print TOPO "</TopologyEntry> \n";

print TOPO "<TopologyEntry> \n";
print TOPO "<ForPhones> \n";
for($i = $sil_num+1; $i < $tol_num; $i++){
	print TOPO "$i ";
}
print TOPO "$tol_num\n";
print TOPO "</ForPhones> \n";

for($i = 0; $i < $state_num; $i++){
	if((($i + 1) % $substate) == 0){
		$pre_state = $i -1;
		$next_state = $i + 1;
		print TOPO "<State> $i <PdfClass> $i <Transition> $pre_state 0.35 <Transition> $i 0.5 <Transition> $next_state 0.15 </State> \n";
	}
	else{
		$next_state = $i + 1;
		print TOPO "<State> $i <PdfClass> $i <Transition> $i 0.75 <Transition> $next_state 0.25 </State> \n";
	}
}
print TOPO "<State> $state_num </State> \n";

print TOPO "</TopologyEntry> \n";
print TOPO "</Topology> \n";
