all : ext2_ls ext2_cp ext2_mkdir ext2_ln ext2_rm

ext2_ls : ext2_ls.o 
	gcc -Wall -g -o ext2_ls ext2_ls.o 

ext2_rm : ext2_rm.o 
	gcc -Wall -g -o ext2_rm ext2_rm.o

ext2_cp : ext2_cp.o 
	gcc -Wall -g -o ext2_cp ext2_cp.o 

ext2_mkdir : ext2_mkdir.o 
	gcc -Wall -g -o ext2_mkdir ext2_mkdir.o 

ext2_ln : ext2_ln.o 
	gcc -Wall -g -o ext2_ln ext2_ln.o 

ext2_rm : ext2_rm.o 
	gcc -Wall -g -o ext2_rm ext2_rm.o 

%.o : %.c
	gcc -Wall -g -c $<

clean:
	rm -f ext2_ls ext2_cp ext2_mkdir ext2_ln ext2_rm *.o *~
