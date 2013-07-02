this code works on the linux 2.6.32.

vfs function:

1.file system init,install,mount and umount

2.create and delete file

3.create and delete directory 

4.edit and modify file


how to use the code:

1.make

This generates a file named samplefs.o

2.insmod samplefs.ko

Load the module as root,you can use lsmod to verify if module load successfully

3.mount -t samplefs any /mnt

Mount the file system

4.umount /mnt

Unmount 

5.rmmod samplefs

Unload the module 
