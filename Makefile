CC=/usr/sfw/bin/gcc

all:
	g++ -Wall thr_cnt_user.cpp -o thr_cnt_user
	$(CC) -Wall -D_KERNEL -ffreestanding -m64 -c thr_cnt.c
	ld -melf64_sparc -r -o thr_cnt thr_cnt.o
	cp thr_cnt /usr/kernel/drv/sparcv9/
	#cp thr_cnt.conf /usr/kernel/drv/
	add_drv thr_cnt
	update_drv thr_cnt
clean:
	rem_drv thr_cnt; rm thr_cnt_user thr_cnt.o thr_cnt /usr/kernel/drv/sparcv9/thr_cnt
