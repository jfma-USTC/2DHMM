in_f=open('feats.scp','r')
out_f=open('utt2spk','w')
while True:
	line=in_f.readline()
	line=line.strip('\n')
	if not line:
		break
	file_name=line.split(' ')[0]
	writer_line=file_name+" "+file_name[0:6]
	print >> out_f,writer_line