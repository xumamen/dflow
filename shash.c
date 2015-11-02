/************************************************************/
/*          SHASH mxu@brocade.com 05/15/2015            	*/
/* Using paired flow to implement symmetric/sticky traffic  */
/*															*/
/************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include  "net.h"
#include "of13.h"
#include "shash.h"
#include <time.h>


//Managed Openflow switch attribute
//#define VDX
#define OF_SWITCH 		"127.0.0.1"
#define OF_CONF_PORT 	6633
#define HASH_BITS 		9	

#define OUTPORT_NUM 3	
//int outports[OUTPORT_NUM]={7,8,9,10};
int outports[OUTPORT_NUM]={1,2,3};

#define LOG_CONSOLE 0
#define LOG_OVS_OFCTL 1
#define LOG_OVS_OFCTL_FILE "./pushpflow"

int sendOFHello (int sockfd, uint32_t xid,uint64_t cookie);
int sendOFBarrier (int sockfd, uint32_t xid,uint64_t cookie);
int sendFlowEntry(int sockfd,uint32_t xid,uint64_t cookie,uint32_t in_port,uint32_t nw_src, uint32_t nw_dst,uint32_t outport,uint16_t idle_time,uint16_t hard_time);
void logFlowEntries(int flow[][6],int total_flow_numi,int flag,uint32_t in_port);


int main(int argc,char ** argv)
{
    int i=0;
	uint32_t xid=0,in_port=-1;
	uint64_t cookie=0;

	#ifdef VDX
	in_port=11;
	#endif
	
    int num=1<<HASH_BITS,total_flow_num=num*num,pair_num=0;
	int flow[total_flow_num][6];
	/*SRC DST Pair_entry Pair_number hash_port*/

    int sockfd;
    struct sockaddr_in ofsw_sock;
    struct hostent *ofsw;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
	{
		printf("[SHASH]: Creating socket failed\n");
		exit(-1);
	}

    ofsw = gethostbyname(OF_SWITCH);

    bzero((char *) &ofsw_sock, sizeof(ofsw_sock));
    ofsw_sock.sin_family = AF_INET;
    bcopy((char *)ofsw->h_addr, (char *)&ofsw_sock.sin_addr.s_addr, ofsw->h_length);
    ofsw_sock.sin_port = htons(OF_CONF_PORT);

    if (connect(sockfd,(struct sockaddr *) &ofsw_sock, sizeof(ofsw_sock)) < 0)
	{
		printf("[SHASH]: connect to OF_SWITCH %s:%d failed\n",OF_SWITCH,OF_CONF_PORT);
		exit(-1);
	}

	memset(flow,-1,total_flow_num*sizeof(int)*6);

	for(i=0;i<total_flow_num;i++)
	{
		flow[i][0]=i/num;
		flow[i][1]=i%num;
		flow[i][2]=flow[i][1]*num+flow[i][0];
		if(flow[flow[i][2]][3]==-1||flow[i][1]==flow[i][2])
		{
			flow[i][3]= pair_num;
			flow[flow[i][2]][3]=pair_num;
			pair_num++;
		}
		flow[i][4]= outports[flow[i][3]%OUTPORT_NUM];
	}
	
	//logFlowEntries(flow,total_flow_num,LOG_CONSOLE,in_port);
	//logFlowEntries(flow,total_flow_num,LOG_OVS_OFCTL,in_port);

	time_t startPushTime,stopPushTime;
	int duration=0;

	startPushTime = time(NULL);

	char buf[70];
	memset(buf,0,70);

	printf("[SHASH]: PUSH FLOW TO OF_SWITCH %s:%d total flows:%d\n",OF_SWITCH,OF_CONF_PORT,total_flow_num);

	sendOFHello(sockfd,xid,cookie);
	xid++;

	for(i=0;i<total_flow_num;i++)
	{
		sendFlowEntry(sockfd,xid,cookie,in_port,flow[i][0],flow[i][1],flow[i][4],0,0);
		{
			buf[(i*70/total_flow_num)]='=';
			buf[(i*70/total_flow_num)+1]='>';

			stopPushTime = time(NULL)+1;
			duration=(int)(stopPushTime-startPushTime);
			printf(" %3d%%  [%-70s      secs:%-4d fps:%-4d flow:%-5d\r",(i+1)*100/total_flow_num,buf,duration,i/duration,i+1);
			fflush(stdout);	
			//usleep(100000);
			xid++;
		}
	}
	printf("\n");
	printf("[SHASH]: SEND Barrier,wait for Barrier reply to get the final fps\n");

	sendOFBarrier(sockfd,xid,cookie);
	stopPushTime = time(NULL)+1;
	duration=(int)(stopPushTime-startPushTime);
	printf(" %3d%%  [%-70s      secs:%-4d fps:%-4d flow:%-5d\n",(i+1)*100/total_flow_num,buf,duration,i/duration,i);

	close(sockfd);
	return 0;
}


void logFlowEntries(int flow[][6],int total_flow_num,int flag, uint32_t in_port)
{
	int i=0, num=sqrt(total_flow_num+1);
	FILE *logFile;
	char nw_src[100],nw_dst[100];
	memset(nw_src,0,100);
	memset(nw_dst,0,100);

	if(flag==LOG_CONSOLE)
	{
		printf("*********************************************************************************\n");
		printf(" Flow[%d]  nw_src/mask    nw_dst/mask    pair_entry     pair_number[%d]  hash_port\n", total_flow_num,num+(total_flow_num-num)/2);
		for(i=0;i<total_flow_num;i++)
		printf(" %3d: %12d/%d %12d/%d %15d %15d %10d\n",i,flow[i][0],(1<<HASH_BITS)-1,flow[i][1],(1<<HASH_BITS)-1,flow[i][2],flow[i][3],flow[i][4]);
		printf("*********************************************************************************\n");
		return;
	}

	if(flag==LOG_OVS_OFCTL)
	{
		if((logFile=fopen(LOG_OVS_OFCTL_FILE, "w+"))==NULL)
		{
			printf("err\n");
		};
		for(i=0;i<total_flow_num;i++)
		{

			sprintf(nw_src,"%d.%d.%d.%d/%d.%d.%d.%d",flow[i][0]>>24&0xff,flow[i][0]>>16&0xff,flow[i][0]>>8&0xff,flow[i][0]&0xff,
										  (num-1)>>24&0xff,(num-1)>>16&0xff,(num-1)>>8&0xff,(num-1)&0xff);
			sprintf(nw_dst,"%d.%d.%d.%d/%d.%d.%d.%d",flow[i][1]>>24&0xff,flow[i][1]>>16&0xff,flow[i][1]>>8&0xff,flow[i][1]&0xff,
										  (num-1)>>24&0xff,(num-1)>>16&0xff,(num-1)>>8&0xff,(num-1)&0xff);
			
		if(in_port==-1)
			fprintf (logFile, "ovs-ofctl -O OPENFLOW13 add-flow tcp:%s:%d \"dl_type=0x0800,nw_src= %s,nw_dst=%s,action=output:%d\"\n",
									OF_SWITCH,
									OF_CONF_PORT,
									nw_src, 
									nw_dst,
									flow[i][4] 
									); 
		else
			fprintf (logFile, "ovs-ofctl -O OPENFLOW13 add-flow tcp:%s:%d \"in_port=%d,dl_type=0x0800,nw_src= %s,nw_dst=%s,action=output:%d\"\n",
									OF_SWITCH,
									OF_CONF_PORT,
									in_port,
									nw_src, 
									nw_dst,
									flow[i][4] 
									); 
		}
		fclose(logFile);
		chmod(LOG_OVS_OFCTL_FILE,S_IRWXU);	
		return;
	}
}

int sendOFHello (int sockfd, uint32_t xid,uint64_t cookie)
{
	int n=0;
    MESSAGE(msg0);
    PACK(msg0, "ofp_hello.header.version", OFP_VERSION, UINT8);
    PACK(msg0, "ofp_hello.header.type", OFPT_HELLO, UINT8);
    PACK(msg0, "ofp_hello.header.length", 8, UINT16); 
    PACK(msg0, "ofp_hello.header.xid", xid, UINT32);

    n = write(sockfd, msg0.data, msg0.offset);
    if (n < 0)
    {
        printf("[SHASH]: send openflow hello msg failed\n");
        return -1;
    }
	return n;

}

int sendOFBarrier (int sockfd, uint32_t xid,uint64_t cookie)
{
	int n=0,i=0;

	char buf[1460];
    MESSAGE(msg2);
    PACK(msg2, "ofp_hello.header.version", OFP_VERSION, UINT8);
    PACK(msg2, "ofp_hello.header.type", OFPT_BARRIER_REQUEST, UINT8);
    PACK(msg2, "ofp_hello.header.length", 8, UINT16); 
    PACK(msg2, "ofp_hello.header.xid", xid, UINT32);

    n = write(sockfd, msg2.data, msg2.offset);
    if (n < 0)
    {
        printf("[SHASH]: send openflow barrier msg failed\n");
        return -1;
    }

	while((n=read(sockfd,buf,1460))>0)
		for(i=0;i<n;i++)
		{
			if((buf[i]=0x04)&&(buf[i+1]==0x15)) return 1;
	
		}
	return n;

}

int sendFlowEntry(int sockfd,uint32_t xid,uint64_t cookie,uint32_t in_port,uint32_t nw_src, uint32_t nw_dst,uint32_t outport,uint16_t idle_time,uint16_t hard_time)
{
	int n=0;

    MESSAGE(msg1);
    PACK(msg1, "ofp_flow_mod.header.version", OFP_VERSION, UINT8);
    PACK(msg1, "ofp_flow_mod.header.type", OFPT_FLOW_MOD, UINT8);

	if(in_port==-1)
	{
    	PACK(msg1, "ofp_flow_mod.header.length", 112, UINT16);
	}
	else
	{
    	PACK(msg1, "ofp_flow_mod.header.length", 120, UINT16);
	}

    PACK(msg1, "ofp_flow_mod.header.xid", xid, UINT32);
    PACK(msg1, "ofp_flow_mod.cookie", cookie, UINT64);
    PACK(msg1, "ofp_flow_mod.cookie_mask", 0x0, UINT64);
    PACK(msg1, "ofp_flow_mod.table_id", 0, UINT8);
    PACK(msg1, "ofp_flow_mod.command", OFPFC_ADD, UINT8);
    PACK(msg1, "ofp_flow_mod.idle_timeout", idle_time, UINT16);
    PACK(msg1, "ofp_flow_mod.hard_timeout", hard_time, UINT16);
    PACK(msg1, "ofp_flow_mod.priority", 32768, UINT16);
    PACK(msg1, "ofp_flow_mod.buffer_id", 0xffffffff, UINT32);
    PACK(msg1, "ofp_flow_mod.out_port", OFPP_ANY, UINT32);
    PACK(msg1, "ofp_flow_mod.out_group", OFPG_ANY, UINT32);
    PACK(msg1, "ofp_flow_mod.flags", 0x0, UINT16);
    PADDING(msg1, 2);


	//OXM definition 

    PACK(msg1, "ofp_flow_mod.match.type", OFPMT_OXM, UINT16);

	if(in_port==-1)
	{
    	PACK(msg1, "ofp_flow_mod.match.length", 34, UINT16);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[0].header", OXM_HEADER(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_TYPE, FALSE, 16), UINT32);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[0].value", 0x0800, UINT16);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[1].header", OXM_HEADER(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_SRC, TRUE, 64), UINT32);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[1].value", nw_src, UINT32);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[1].mask", (1<<HASH_BITS)-1, UINT32);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[2].header", OXM_HEADER(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_DST, TRUE, 64), UINT32);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[2].value", nw_dst, UINT32);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[2].mask", (1<<HASH_BITS)-1, UINT32);
    		PADDING(msg1, OFP_MATCH_OXM_PADDING(34));
	}
	else
	{
    	PACK(msg1, "ofp_flow_mod.match.length", 42, UINT16);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[0].header", OXM_HEADER(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_ETH_TYPE, FALSE, 16), UINT32);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[0].value", 0x0800, UINT16);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[1].header", OXM_HEADER(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IN_PHY_PORT, FALSE, 32), UINT32);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[1].value", in_port, UINT32);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[2].header", OXM_HEADER(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_SRC, TRUE, 64), UINT32);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[2].value", nw_src, UINT32);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[2].mask", (1<<HASH_BITS)-1, UINT32);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[3].header", OXM_HEADER(OFPXMC_OPENFLOW_BASIC, OFPXMT_OFB_IPV4_DST, TRUE, 64), UINT32);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[3].value", nw_dst, UINT32);
        	PACK(msg1, "ofp_flow_mod.match.oxm_fields[3].mask", (1<<HASH_BITS)-1, UINT32);
    		PADDING(msg1, OFP_MATCH_OXM_PADDING(42));
	}


	//ACTION definition

    PACK(msg1, "ofp_flow_mod.instructions[0].type", OFPIT_APPLY_ACTIONS, UINT16);
    PACK(msg1, "ofp_flow_mod.instructions[0].len", 24, UINT16);
    PADDING(msg1, 4);
        PACK(msg1, "ofp_flow_mod.instructions[0].actions[0].type", OFPAT_OUTPUT, UINT16);
        PACK(msg1, "ofp_flow_mod.instructions[0].actions[0].len", 16, UINT16);
        PACK(msg1, "ofp_flow_mod.instructions[0].actions[0].port", outport, UINT32);
        PACK(msg1, "ofp_flow_mod.instructions[0].actions[0].max_len", 0, UINT16);
		PADDING(msg1,6);

    n = write(sockfd, msg1.data, msg1.offset);
    if (n < 0)
	{
		printf("[SHASH]: send openflow msg failed\n");
		return -1;
	}
	return 0;
}
