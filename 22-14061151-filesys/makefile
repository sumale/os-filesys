filesys: filesys.c filesys.h
	cc -o filesys filesys.c filesys.h
	dd if=/dev/zero of=data bs=256M count=1
	mkfs.fat -F 16 data
clean:
	rm filesys data
