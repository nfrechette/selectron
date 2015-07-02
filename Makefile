HSA_DEVEL_ROOT=\Users\pcwalton\Applications\HSA-Devel-Beta\sdk
#CFLAGS=/Zi
#CFLAGS=-Os -Wall -arch i386
CFLAGS=-Os -Wall -arch x86_64

all:	selectron-cl.exe

#selectron-cl.exe:	selectron-cl.cpp selectron.h
#	cl $(CFLAGS) /I$(HSA_DEVEL_ROOT)\include selectron-cl.cpp /link $(HSA_DEVEL_ROOT)\lib\x86\OpenCL.lib

selectron-cl.exe:	selectron-cl.cpp selectron.h
	clang $(CFLAGS) -framework OpenCL -o selectron-cl selectron-cl.cpp

