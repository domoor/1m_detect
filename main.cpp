#include <winsock2.h>
#include <Ws2tcpip.h>		// inet_ntop()
#include <windows.h>
#include <stdio.h>		// printf()
#include <stdint.h>		// uintN_t
#include <stdlib.h>
#include <iostream>
#include <string>
#include <regex>
#include <fstream>
#include <vector>
#include "windivert.h"
using namespace std;

#define MAXBUF			0xFFFF
#define IPv4			4
#define PROTOCOL_TCP		6
#define PORT_HTTP		80
#define SYMBOL			39
#define NUMBER			1000001

#define chk_flag		1

uint8_t changeASCII[] // 0[0]~9[9] | A(a)[10]~Z(z)[35] | .[36] -[37] _[38]
= { 37,36,255,0,1,2,3,4,5,6,7,8,9,255,255,255,255,255,255,255,
10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,
255,255,255,255,38,255,
10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35 };
int sum = 0;

struct ipv4_hdr
{
	uint8_t HL  : 4,			/* header length */
		Ver : 4;			/* version */
	uint8_t tos;				/* type of service */
	uint16_t len;				/* total length */
	uint16_t id;				/* identification */
	uint16_t off;				/* offset */
	uint8_t ttl;				/* time to live */
	uint8_t protocol;			/* protocol */
	uint16_t chk;				/* checksum */
	struct in_addr src, dst;		/* source and dest address */
};

struct tcp_hdr
{
	uint16_t s_port;			/* source port */
	uint16_t d_port;			/* destination port */
	uint32_t seq;				/* sequence number */
	uint32_t ack;				/* acknowledgement number */
	uint8_t reservation : 4,		/* (unused) */
		off         : 4;		/* data offset */
	uint8_t  flag;				/* control flags */
	uint16_t windows;			/* window */
	uint16_t chk;				/* checksum */
	uint16_t urgent_P;			/* urgent pointer */
};
int ToNumber(char ch) {
	return changeASCII[ch-'-'];
}
// 35 minutes for 8756127 nodes to be created.
// memory is used 1400 MB.
// node of trie
struct TrieNode {
	TrieNode* children[SYMBOL];
	// this node is terminal
	bool terminal;
	TrieNode() : terminal(false) {
		memset(children, 0, sizeof(children));
	}
	~TrieNode() {
		for (int i = 0; i < SYMBOL; ++i)
			if (children[i])
				delete children[i];
	}
	// key add.
	void insert(string key) {
		if (key[0] == 0) {
			terminal = true;
		}
		else {
			int next = ToNumber(key[0]);
			if (children[next] == NULL) {
				sum++;
				children[next] = new TrieNode();
			}
			children[next]->insert(&key[1]);
		}
	}

	TrieNode* find(string key) {
		if (key[0] == 0)
			return this;
		int next = ToNumber(key[0]);
		if (children[next] == NULL)
			return NULL;
		return children[next]->find(&key[1]);
	}
};


void check(struct ipv4_hdr *ip, struct tcp_hdr *tcp) {
	uint8_t ip_buf[INET_ADDRSTRLEN];
	printf("Source IP\t: %s\n", inet_ntop(AF_INET, &ip->src, (char*)ip_buf, INET_ADDRSTRLEN));
	printf("Destination IP\t: %s\n", inet_ntop(AF_INET, &ip->dst, (char*)ip_buf, INET_ADDRSTRLEN));
	printf("Source Port\t: %u\n", ntohs(tcp->s_port));
	printf("Destination Port: %u\n", ntohs(tcp->d_port));
	printf("Sequence Number\t: ");
	for(int i=0; i<4; i++) printf("%02X ", ((uint8_t*)&tcp->seq)[i]);
	puts("");
}

void dump(uint8_t *pkt, uint32_t size){
    for(int i=0; i<size; i++) {
        if(i && i%16==0) puts("");
        printf("%02X ", pkt[i]);
    }
	puts("");
}

int main() {
	TrieNode* ptn = new TrieNode();
	int idx = 1;
	ifstream in("majestic_million.csv");
	string s;
	if(in.is_open()){
		while(!in.eof()) {
			getline(in, s);
			regex pattern("[^,]+[.]+[^,]+");
			smatch m;
			if(regex_search(s,m,pattern)) {
				string URL = m.str();
				ptn->insert(URL);
				cout << idx++ << " : " << URL << endl;
			}
		}
	}
	else cout << "file not found!\n";
	in.close();
	printf("nodes : %d\n", sum);

	uint16_t priority = 0;
	HANDLE handle = WinDivertOpen("true", WINDIVERT_LAYER_NETWORK, priority, 0);
	if (handle == INVALID_HANDLE_VALUE)	{
		fprintf(stderr, "error: failed to open the WinDivert device\n");
		return 1;
	}
	while(TRUE) {
		uint8_t packet[MAXBUF];
		uint32_t packet_len;
		WINDIVERT_ADDRESS addr;
		if(!WinDivertRecv(handle, packet, sizeof(packet), &addr, &packet_len)) {
			fprintf(stderr, "warning: failed to read packet\n");
			continue;
		}

		struct ipv4_hdr *ip = (struct ipv4_hdr*)packet;
		if(ip->Ver==IPv4 && ip->protocol==PROTOCOL_TCP) {
			uint32_t ip_len = ip->HL<<2;
			struct tcp_hdr *tcp = (struct tcp_hdr*)(packet+ip_len);
			if(ntohs(tcp->s_port)==PORT_HTTP || ntohs(tcp->d_port)==PORT_HTTP) {
				uint32_t tcp_len = tcp->off<<2;
				uint32_t data_len = ntohs(ip->len)-ip_len-tcp_len;
				uint8_t *data = (uint8_t*)tcp+tcp_len;
				if(data_len) {
					string payload(data, data+data_len);
					regex pattern("Host: ([^\n]+)");
					smatch m;
					puts("on");
					if(regex_search(payload,m,pattern)) {
						string URL = m[1].str();
						uint32_t URL_len = URL.length();
						URL.erase(URL_len-1);
						TrieNode* ptn0 = ptn->find(URL);
						if(ptn0) {
							printf("\nBlocked URL : %s\n", URL.c_str());
							if(chk_flag) check(ip, tcp);
							if(chk_flag) dump(data, 40);
							continue;
						}
					}
				}
			}
		}

		if(!WinDivertSend(handle, packet, packet_len, &addr, NULL)) {
			fprintf(stderr, "warning: failed to send\n");
		}
	}
}
