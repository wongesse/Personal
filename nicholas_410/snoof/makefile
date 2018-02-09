#makefile

all: sniffex snoofer spoof_icmp

sniffex: sniffex.o
	gcc sniffex.c -lpcap -o sniffer
spoof_icmp: spoof_icmp.o
	gcc spoof_icmp.c -o spoof_icmp

snoofer: snoofer.o
	gcc snoofer.c -lpcap -o snoofer
clean:
	rm -rf *.o sniffer spoof_icmp snoofer
