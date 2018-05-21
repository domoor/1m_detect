#include <winsock2.h>
#include <Ws2tcpip.h>		// inet_ntop()
#include <windows.h>
#include <cstdio>		// printf()
#include <cstdint>		// uintN_t
#include <cstdlib>
//#include <iostream>		// cout
#include <string>		// string
#include <regex>		// regex
#include <fstream>		// file in/out
#include "windivert.h"
using namespace std;

#define MAXBUF			0xFFFF
#define IPv4			4
#define PROTOCOL_TCP		6
#define PORT_HTTP		80
#define MAX_NUMBER		1000001
#define half(x, y)		((x)+(y))/2

uint32_t sum = 0;
uint32_t fileNum[MAX_NUMBER];
string str_array[MAX_NUMBER];

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

bool file_csv() {
	uint32_t idx = 0;
	ifstream in;
	ofstream out;
//	FILE *rfp, *wfp;

	in.open("majestic_million.csv");
//	rfp = fopen("majestic_million.csv", "r");
	string s;
	if(in.is_open()) {
//	if(rfp != NULL){
//		char s_tmp[1024];
		while (!in.eof()) {
//		for(idx=0; NULL != fgets(s_tmp,1024,rfp); idx++){
			getline(in, s);
			regex pattern("[^,]+[.]+[^,]+");
			smatch m;
//			s = s_tmp;
			if(regex_search(s, m, pattern)) {
				string URL = m.str();
				str_array[idx] = URL;
				// 14783570 개 + 포인트배열 1000001개 + uint32_t 1000001개 = 22783574 바이트 / 최대 22MB 소요 예상
				sum += str_array[idx].size();
				printf("%d : %s\n",idx+1, str_array[idx].c_str());
				idx++;
			}
		}
	}
	else { fprintf(stderr, "error: File not found\n"); return 1; }
	in.close();
//	fclose(rfp);
	printf("\nstring size : %d\n", sum);

	puts("\nsorting");
	sort(str_array, str_array + MAX_NUMBER);
	puts("sorted\n");

	// 정렬된 URL 저장하기.
	out.open("sorted_url.txt");
	if(out.is_open()) {
		for(uint32_t i=0; i<MAX_NUMBER; i++) out << str_array[i] <<'\n';
	}
	else { fprintf(stderr, "error: creating file\n"); return 1; }
	out.close();

	// 정렬된 URL 포인터 저장하기.
	out.open("sorted_url_num.txt");
	if(out.is_open()) {
		uint32_t n = 0;
		for(uint32_t i=0; i<MAX_NUMBER; i++) {
			out << n << '\n';
			fileNum[i] = n;
			n += str_array[i].size()+2;
		}
	}
	else { fprintf(stderr, "error: creating file\n"); return 1; }
	out.close();

	return 0;
}

bool file_txt() {
	ifstream in("sorted_url_num.txt");
	string s_tmp;

	puts("\nsorted_num_loading");
	if(in.is_open()) {
		for(uint32_t i=0; i<MAX_NUMBER; i++) {
			getline(in, s_tmp);
			fileNum[i] = stoi(s_tmp);
		}
	}
	else { fprintf(stderr, "error: File not found\n"); return 1; }
	in.close();
	puts("load complete.\n");

	return 0;
}

bool binarysearch(ifstream* in, string URL) {
	uint32_t max_p = MAX_NUMBER;
	uint32_t min_p = 0;
	uint32_t now_p = half(max_p,min_p);
	while(1){
		if(str_array[now_p].empty()) { 			// 비었으면
			string s_tmp;
			(*in).seekg(fileNum[now_p]);
			getline(*in, s_tmp);
			str_array[now_p] = s_tmp;
		}
		int res;
		if((res = URL.compare(str_array[now_p]))==0) {	// 일치하면
			return true;
		}
		else if(max_p-min_p == 1) {			// 필터 대상 X
			return false;
		}
		else if(res < 0) { 				// 사전순으로 앞일 때
			max_p = now_p;
			now_p = half(max_p, min_p);
		}
		else if(res > 0) { 				// 사전순으로 뒤일 때
			min_p = now_p;
			now_p = half(max_p, min_p);
		}
		else { fprintf(stderr, "error: occurred during search\n"); exit(1); }
	}
}

int main() {
	ifstream in;
	uint32_t switched;

	puts("[ used file ]");
	puts("\"majestic_million.csv\"\t= 1");
	puts("\"sorted_*.txt\"\t\t= 2");
	printf("Please enter : ");
	scanf("%u", &switched);

	switch(switched){
	case 1:
		if(file_csv()) { fprintf(stderr, "error: program exit\n"); return 1; }
		break;
	case 2:
		if(file_txt()) { fprintf(stderr, "error: program exit\n"); return 1; }
		break;
	default:
		fprintf(stderr, "error: Incorrect input\n");
		return 1;
	}

	in.open("sorted_url.txt");
	if(!in.is_open()) { fprintf(stderr, "error: File not found\n"); return 1; }

	puts("[ WinDivert Start! ]\n");

	uint16_t priority = 0;
	HANDLE handle = WinDivertOpen("true", WINDIVERT_LAYER_NETWORK, priority, 0);
	if(handle == INVALID_HANDLE_VALUE)	{
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
					if(regex_search(payload,m,pattern)) {
						string URL = m[1].str();
						uint32_t URL_len = URL.length();
						URL.erase(URL_len-1);
						if(binarysearch(&in, URL)) {
							printf("\nBlocked URL : %s\n", URL.c_str());
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
	in.close();
}
