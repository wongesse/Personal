#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/ether.h>


#include <net/ethernet.h>
#include <net/if.h>

#include <linux/if_packet.h>
#include <arpa/inet.h>
#include <time.h>

#define DATA_SIZE 100

typedef struct PseudoHeader{

	unsigned long int source_ip;
	unsigned long int dest_ip;
	unsigned char reserved;
	unsigned char protocol;
	unsigned short int tcp_length;

}PseudoHeader;

unsigned short computeCheckSum(unsigned char *data, int len) {
    long sum = 0;  /* assume 32 bit long, 16 bit short */
 	unsigned short *temp = (unsigned short *)data;

	while(len > 1){
		sum += *temp++;
		if(sum & 0x80000000)   /* if high order bit set, fold */
			sum = (sum & 0xFFFF) + (sum >> 16);
		len -= 2;
	}

	if(len)       /* take care of left over byte */
		sum += (unsigned short) *((unsigned char *)temp);

	while(sum>>16)
		sum = (sum & 0xFFFF) + (sum >> 16);

	return ~sum;
}

int CreatePseudoHeaderAndComputeTcpChecksum(struct tcphdr *tcp_header, struct iphdr *ip_header, unsigned char *data)
{
	/*The TCP Checksum is calculated over the PseudoHeader + TCP header +Data*/

	/* Find the size of the TCP Header + Data */
	int segment_len = ntohs(ip_header->tot_len) - ip_header->ihl*4; 

	/* Total length over which TCP checksum will be computed */
	int header_len = sizeof(PseudoHeader) + segment_len;

	/* Allocate the memory */

	unsigned char *hdr = (unsigned char *)malloc(header_len);

	/* Fill in the pseudo header first */
	
	PseudoHeader *pseudo_header = (PseudoHeader *)hdr;

	pseudo_header->source_ip = ip_header->saddr;
	pseudo_header->dest_ip = ip_header->daddr;
	pseudo_header->reserved = 0;
	pseudo_header->protocol = ip_header->protocol;
	pseudo_header->tcp_length = htons(segment_len);

	
	/* Now copy TCP */

	memcpy((hdr + sizeof(PseudoHeader)), (void *)tcp_header, tcp_header->doff*4);

	/* Now copy the Data */

	memcpy((hdr + sizeof(PseudoHeader) + tcp_header->doff*4), data, DATA_SIZE);

	/* Calculate the Checksum */

	tcp_header->check = computeCheckSum(hdr, header_len);

	/* Free the PseudoHeader */
	free(hdr);

	return -1;

}

int spoof() {

	char *device = "enp0s3";
	int sd;
	unsigned char* packet;
	struct ethhdr * ethernet_header;
	struct iphdr * ip_header;
	struct tcphdr * tcp_header;
	unsigned char * data;
	int pkt_len;

	// create raw socket

	if ((sd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1 ) {
		perror("Error creating raw socket: ");
		return -1;
	}

	// binding raw socket to interface

	struct sockaddr_ll sll;
	struct ifreq ifr;

	bzero(&sll, sizeof(sll));
	bzero(&ifr, sizeof(ifr));

	strncpy((char *) ifr.ifr_name, device, IFNAMSIZ);

	if ((ioctl(sd, SIOCGIFINDEX, &ifr)) == -1) {
	        printf("Error getting Interface index!\n");
	        return -1;
	}

	sll.sll_family = AF_PACKET;
	sll.sll_ifindex = ifr.ifr_ifindex;
	sll.sll_protocol = htons(ETH_P_ALL);

	if ((bind(sd, (struct sockaddr *) &sll, sizeof(sll))) == -1) {
	        printf("Error binding raw socket to interface\n");
	        return -1;
	}

	// creating ethernet header

	ethernet_header = (struct ethhdr *) malloc(sizeof(struct ethhdr));
	// src mac address
	memcpy(ethernet_header->h_source, (void *) ether_aton("08:00:27:da:f4:fc"), 6);
	// dst mac address
	memcpy(ethernet_header->h_dest, (void *) ether_aton("ff:ff:ff:ff:ff:ff"), 6);
	// protocol
	ethernet_header->h_proto = htons(ETHERTYPE_IP);

	// create ip header

	ip_header =  (struct iphdr*) malloc(sizeof(struct iphdr));
	ip_header->version = 4;
	ip_header->ihl = (sizeof(struct iphdr))/4;
	ip_header->tos = 0;
	ip_header->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr) + DATA_SIZE);
	ip_header->id = htons(111);
	ip_header->frag_off = 0;
	ip_header->ttl = 63;
	ip_header->protocol = IPPROTO_ICMP;
	ip_header->check = 0;
	ip_header->saddr = inet_addr("111.111.0.3");
	ip_header->daddr = inet_addr("10.230.195.217");

	ip_header->check = computeCheckSum((unsigned char *) ip_header, ip_header->ihl*4);

	// create tcp header

	tcp_header = (struct tcphdr *) malloc (sizeof(struct tcphdr));
	tcp_header->source = htons(80);
	tcp_header->dest = htons(10000);
	tcp_header->seq = htonl(111);
	tcp_header->ack_seq = htonl(111);
	tcp_header->res1 = 0;
	tcp_header->doff = (sizeof(struct tcphdr))/4;
	tcp_header->syn = 1;
	tcp_header->window = htons(100);
	tcp_header->check = 0;
	tcp_header->urg_ptr = 0;

	// create data

	data = (unsigned char *) malloc(DATA_SIZE);
	struct timeval tv;
	struct timezone tz;
	int counter = DATA_SIZE;

	gettimeofday(&tv, &tz);
	srand(tv.tv_sec);

	for (counter = 0; counter < DATA_SIZE; counter++) {
		data[counter] = 255.0 * rand()/(RAND_MAX + 1.0);
	}

	CreatePseudoHeaderAndComputeTcpChecksum(tcp_header, ip_header, data);
	pkt_len = sizeof(struct ethhdr) + ntohs(ip_header->tot_len);
	packet = (unsigned char *) malloc(pkt_len);

	memcpy(packet, ethernet_header, sizeof(struct ethhdr));
	memcpy((packet + sizeof(struct ethhdr)), ip_header, ip_header->ihl*4);
	memcpy((packet + sizeof(struct ethhdr) + ip_header->ihl*4), tcp_header, tcp_header->doff*4);
	memcpy((packet + sizeof(struct ethhdr) + ip_header->ihl*4 + tcp_header->doff*4), data, DATA_SIZE);

	int send;
	if ((send = write(sd, packet, pkt_len)) != pkt_len) {
		printf("Error writing\n");
	}
	else {
		printf("Packet sent!\n");
	}

	free(ethernet_header);
	free(ip_header);
	free(tcp_header);
	free(data);
	free(packet);
	close(sd);

	return 0;




}

int main() {
	spoof();
	return 0;
}