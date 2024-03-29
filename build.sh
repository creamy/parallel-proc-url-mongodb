#only need env_posix.c on this machine
rm -f /usr/home/waitman/downloads/mongo-c-driver/src/env_win32.c
rm -f /usr/home/waitman/downloads/mongo-c-driver/src/env_standard.c
gcc46 --std=c99 -Wall -Wno-unused-function \
	/usr/home/waitman/downloads/mongo-c-driver/src/*.c \
	tmo.c \
        -I/usr/home/waitman/downloads/mongo-c-driver/src/ \
	-I/usr/local/include \
	`xml2-config --cflags` \
	`xml2-config --libs` \
	-L/usr/local/lib -lcurl -lpthread -o tmo
