# gepasp makefile
CC=gcc
TARGET=main
OPTIMIZATION=3
COPTS=-g -mcpu=cortex-a53 -mfpu=neon -mfloat-abi=hard -marm -o $(TARGET) -O$(OPTIMIZATION)

all: bin

bin: main.c window.S zoom.S windowSISD.S zoomSISD.S zoomIS.S
	$(CC) $(COPTS) $+ 

clean:
	rm $(TARGET)
