#include  <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "rtsdk.h"

extern int rtonboot_shutdown;

extern unsigned char * p_bulk;

int is_send_result_msg_received = 0;

int is_testbc_msg_received = 0;

int is_testbr_msg_received = 0;

int is_foe_read_msg_received = 0;

int is_testsl_msg_received = 0;

uint32_t foe_read_size;

void end_callack_execcmd_result(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	struct send_result_msg_content * p_send_result;
	int ret;
	char * buf_ptr;
	char * buf_ptr_ori;
	uint32_t cur_pos;
	int len;
	
	p_send_result = (struct send_result_msg_content *)(p_content);
	
	if(!(p_send_result->non_tty_debug))
		goto return_from_execcmd_result;
	
	if(!(p_send_result->non_tty_cmd_result_buflen))
			goto return_from_execcmd_result;
			
	buf_ptr = malloc(p_send_result->non_tty_cmd_result_buflen);
	if(!buf_ptr)
	{
		printf("Winfred Young end_callack_execcmd_result cannot malloc buf \n");
		
		goto return_from_execcmd_result;
	}
	
	buf_ptr_ori = buf_ptr;
	
	ret = RTSDKCpyKernelBuffer(p_send_result->non_tty_cmd_result_bufoff,
		p_send_result->non_tty_cmd_result_buflen, buf_ptr);
	if(ret != ERROR_NUM_OK)
	{
		printf("Winfred Young end_callack_execcmd_result RTSDKCpyKernelBuffer fail \n");
		
		free(buf_ptr_ori);
		
		goto return_from_execcmd_result;
	}
	
	cur_pos = 0;
	while(1)
	{
		if( cur_pos >= (p_send_result->non_tty_cmd_result_buflen) )
			break;
			
		len = strlen(buf_ptr);
				
		printf("%s", buf_ptr);
		
		cur_pos += len + 1;
		
		buf_ptr += len + 1;	
	};	
	
	free(buf_ptr_ori);
	
	return_from_execcmd_result:
	
		is_send_result_msg_received = 1;
}

void user_broadcast_callback(uint32_t msgid, uint32_t msg_len, uint8_t * p_content)
{
	is_testbc_msg_received = 1;
	
	printf("Winfred Young is_testbc_msg_received \n");
}	

void end_callack_testbr(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{
	uint32_t * ptemp = (uint32_t *)(p_bulk);
     
	#ifdef RTONBOOT_DEBUG
     
			printf("Winfred Young rtos bulk head 32 is %lx \n", *(ptemp) );
		
	#endif
			
	is_testbr_msg_received = 1;
}

void end_callack_foe_read(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{		
	struct rtonboot_foe_read_para * p_foe_read_para = (struct rtonboot_foe_read_para *)(p_content);
	
	foe_read_size = p_foe_read_para->file_size;
	
	is_foe_read_msg_received = 1;
}

void end_callack_testsl(uint32_t msgid, uint32_t msglen, uint8_t * p_content, void * priv)
{		
	struct rtonboot_test_slave * p_test_slave = (struct rtonboot_test_slave *)(p_content);
	
	#ifdef RTONBOOT_DEBUG
     
			printf("Winfred Young testsl magic is %lx \n", p_test_slave->magic );
		
	#endif
	
	is_testsl_msg_received = 1;
}

void user_set_end_callback(uint32_t dst_msgid, rtsdk_end_callack_func_t * p_cb, void ** p_priv)
{
	switch (dst_msgid)
	{
		case MSGID_SEND_EXECCMD_RESULT:
		
			*p_cb = &end_callack_execcmd_result;
			
			*p_priv = NULL;
		
			break;
			
		case MSGID_RTOS_BULK_READY_MSG:
		
			*p_cb = &end_callack_testbr;
			
			*p_priv = NULL;
		
			break;
			
		case MSGID_FOE_READ:
		
			*p_cb = &end_callack_foe_read;
			
			*p_priv = NULL;
		
			break;	
			
		case MSGID_TEST_SLAVE:
		
			*p_cb = &end_callack_testsl;
			
			*p_priv = NULL;
		
			break;	
	};
}

struct rtonboot_rr_resp_header rr_resp_header;

void resp_callack_testrr(uint32_t msgid, uint32_t * p_msglen, uint8_t ** pp_content, 
	uint32_t max_content_size, void * priv)
{
	*p_msglen = sizeof(struct rtonboot_rr_resp_header);
	
	rr_resp_header.resp_status = 0x88;
	
	*(pp_content) = (uint8_t *)(&rr_resp_header);
}

void user_set_resp_callback(uint32_t dst_msgid, rtsdk_resp_callack_func_t * p_cb, void ** p_priv)
{
	switch (dst_msgid)
	{
		case MSGID_TESTRR:
		
			*p_cb = &resp_callack_testrr;
			
			*p_priv = NULL;
		
			break;
	};
}

struct rtonboot_bulkpara linux_bulkbuf_para;

void prepare_callack_linux_bulk_ready(uint32_t msgid, uint32_t * p_msglen, uint8_t ** pp_content, 
	uint32_t max_content_size, void * priv)
{
	(*p_msglen) = 8;
	
	linux_bulkbuf_para.offset = 0;
	linux_bulkbuf_para.len = 8;
	
	*(pp_content) = (uint8_t *)(&linux_bulkbuf_para);
}

uint8_t rtos_cmd_buf[512];

void prepare_callack_execcmd(uint32_t msgid, uint32_t * p_msglen, uint8_t ** pp_content, 
	uint32_t max_content_size, void * priv)
{
	char * p_rtos_cmd;
	
	p_rtos_cmd = &rtos_cmd_buf[4];
	
	(*p_msglen) = strlen(p_rtos_cmd) + 5;
	
	*(pp_content) = &rtos_cmd_buf[0];
}

long file_size;

struct rtonboot_foe_write_para foe_write_para;

struct rtonboot_sii_write_para sii_write_para;

void prepare_callack_foe_wrire(uint32_t msgid, uint32_t * p_msglen, uint8_t ** pp_content, 
	uint32_t max_content_size, void * priv)
{
	foe_write_para.file_size = file_size;
	
	(*p_msglen) = sizeof(struct rtonboot_foe_write_para);
	
	*(pp_content) = &foe_write_para;
}

void prepare_callack_sii_wrire(uint32_t msgid, uint32_t * p_msglen, uint8_t ** pp_content, 
	uint32_t max_content_size, void * priv)
{
	sii_write_para.file_size = file_size;
	
	(*p_msglen) = sizeof(struct rtonboot_sii_write_para);
	
	*(pp_content) = &sii_write_para;
}

void user_set_prepare_callback(uint32_t dst_msgid, rtsdk_prepare_send_callack_func_t * p_cb, void ** p_priv)
{
	switch (dst_msgid)
	{
		case MSGID_LINUX_BULK_READY_MSG:
		
			*p_cb = &prepare_callack_linux_bulk_ready;
			
			*p_priv = NULL;
		
			break;
		
		case MSGID_EXECCMD_ON_RTOS:
		
			*p_cb = &prepare_callack_execcmd;
			
			*p_priv = NULL;
		
			break;
			
		case MSGID_FOE_WRITE:
		
			*p_cb = &prepare_callack_foe_wrire;
			
			*p_priv = NULL;
		
			break;	
			
		case MSGID_SII_WRITE:
		
			*p_cb = &prepare_callack_sii_wrire;
			
			*p_priv = NULL;
		
			break;		
	};	
}

struct rtonboot_create_msg_desc cmds_msgs[] = {
	{	
		.dst_msgid = MSGID_EXECCMD_ON_RTOS,
		.max_content_size = 512,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	},	
    
    { 			
		.dst_msgid = MSGID_SEND_EXECCMD_RESULT,
		.max_content_size = 64,
		.msg_origin = MSG_ORIGIN_RTOS,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	}	
};

struct rtonboot_create_msg_desc testrr_msgs[] = {
	{
		.dst_msgid = MSGID_TESTRR,
		.max_content_size = 64,
		.msg_origin = MSG_ORIGIN_RTOS,
		.msg_has_response = MSG_HAS_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,	
	}	
};

struct rtonboot_create_msg_desc testk3_msgs[] = {
	{
		.dst_msgid = MSGID_TESTK3,
		.max_content_size = 0,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_HAS_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 3,
	}	
};

struct rtonboot_create_msg_desc foe_write_msgs[] = {
	{
		.dst_msgid = MSGID_FOE_WRITE,
		.max_content_size = 8,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	}	
};

struct rtonboot_create_msg_desc foe_read_msgs[] = {
	{
		.dst_msgid = MSGID_FOE_READ,
		.max_content_size = 8,
		.msg_origin = MSG_ORIGIN_RTOS,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	}	
};

struct rtonboot_create_msg_desc sii_write_msgs[] = {
	{
		.dst_msgid = MSGID_SII_WRITE,
		.max_content_size = 8,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	}	
};

struct rtonboot_create_msg_desc testsl_msgs[] = {
	{
		.dst_msgid = MSGID_TEST_SLAVE,
		.max_content_size = 16,
		.msg_origin = MSG_ORIGIN_RTOS,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 0,
	}	
};

struct rtonboot_create_msg_desc rteth_msgs[] = {
	{
		.dst_msgid = MSGID_RTETH_SEND,
		.max_content_size = 0,
		.msg_origin = MSG_ORIGIN_LINUX,
		.msg_has_response = MSG_HAS_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 7,
	},
	
	{
		.dst_msgid = MSGID_RTETH_RECV,
		.max_content_size = SINGLE_RTETH_BUFFER_SIZE + 4,
		.msg_origin = MSG_ORIGIN_RTOS,
		.msg_has_response = MSG_NO_RESPONSE,
		.is_broadcast = 0,
		.is_kernel = 1,
	}	
};

long fsize(FILE * fp)
{
	long n;
	fpos_t fpos; 
	
	fgetpos(fp, &fpos);
	
	fseek(fp, 0, SEEK_END);
	n = ftell(fp);
	
	fsetpos(fp,&fpos);
	
	return n;
}

int getOptions(int argc, char **argv)
{
    int c, argCount;
    
    optind = 0;
    
    static struct option longOptions[] = {
        //name,         has_arg,           flag, val
        {"master",      required_argument, NULL, 'm'},
        {"alias",       required_argument, NULL, 'a'},
        {"position",    required_argument, NULL, 'p'},
        {"domain",      required_argument, NULL, 'd'},
        {"type",        required_argument, NULL, 't'},
        {"output-file", required_argument, NULL, 'o'},
        {"skin",        required_argument, NULL, 's'},
        {"emergency",   no_argument,       NULL, 'e'},
        {"force",       no_argument,       NULL, 'f'},
        {"quiet",       no_argument,       NULL, 'q'},
        {"verbose",     no_argument,       NULL, 'v'},
        {"help",        no_argument,       NULL, 'h'},
        {}
    };
    
    do {
		
		c = getopt_long(argc, argv, "m:a:p:d:t:o:s:efqvh", longOptions, NULL);

		switch (c) {
            case 'm':
                
                break;

            case 'a':
                
                break;

            case 'p':
                
                break;

            case 'd':
                
                break;

            case 't':
                
                break;

            case 'o':
                
                break;

            case 's':
                
                break;

            case 'e':
                
                break;

            case 'f':
                
                break;

            case 'q':
                
                break;

            case 'v':
                
                break;

            case 'h':
                
                break;

            case '?':
                
                break;

            default:
                break;
        };
    }
    while (c != -1);

    argCount = argc - optind;

    return argCount;
}

char rteth_buffer[1024];
int sockfd_send = -1;
int sockfd_recv = -1;

struct sockaddr_in dest_addr;


#define DEST_IP "192.168.1.188"
#define SRC_IP "192.168.1.108"
    
#define DEST_PORT 8080
#define SRC_PORT 12345

#define MAX_RTETH_TEST_COUNT 100

unsigned short checksum(unsigned short *ptr, int nbytes) {
    register long sum;
    unsigned short oddbyte;
    register short answer;

    sum = 0;
    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1) {
        oddbyte = 0;
        *((u_char*)&oddbyte) = *(u_char*)ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    answer = (short)~sum;
    
    return answer;
}

void create_tcp_packet(char *buffer, const char *data) {
    struct iphdr *iph = (struct iphdr*)buffer;
    struct tcphdr *tcph = (struct tcphdr*)(buffer + sizeof(struct iphdr));
    char *payload = buffer + sizeof(struct iphdr) + sizeof(struct tcphdr);
    int len = strlen(data);
    
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr) + len + 1;
    iph->id = htons(54321);
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_TCP;
    iph->check = 0;
    iph->saddr = inet_addr(SRC_IP);
    iph->daddr = inet_addr(DEST_IP);
    iph->check = checksum((unsigned short*)buffer, sizeof(struct iphdr));
    
    tcph->source = htons(SRC_PORT);
    tcph->dest = htons(DEST_PORT);
    tcph->seq = htonl(1);
    tcph->ack_seq = 0;
    tcph->doff = 5;
    tcph->fin = 0;
    tcph->syn = 1;
    tcph->rst = 0;
    tcph->psh = 0;
    tcph->ack = 0;
    tcph->urg = 0;
    tcph->window = htons(5840);
    tcph->check = 0;
    tcph->urg_ptr = 0;
    
    struct pseudo_header {
        u_int32_t source_address;
        u_int32_t dest_address;
        u_int8_t placeholder;
        u_int8_t protocol;
        u_int16_t tcp_length;
    } psh;
    
    psh.source_address = inet_addr(SRC_IP);
    psh.dest_address = inet_addr(DEST_IP);
    psh.placeholder = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(sizeof(struct tcphdr) + len + 1);
    
    int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr) + len + 1;
    
    char * pseudogram = (char *)(malloc(psize));
    memcpy(pseudogram, (char*)&psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr) + len + 1);
    
    tcph->check = checksum((unsigned short*)pseudogram, psize);
    free(pseudogram);
    
    memcpy(payload, data, len + 1);
}

int prepare_test_rteth_and_send(int loopcnt)
{
	char data[80];
	
    memset(rteth_buffer, 0, sizeof(rteth_buffer));
    
    sprintf(&data[0], "Hello from rteth test %d", loopcnt);
    
    create_tcp_packet(rteth_buffer, data);
	
	if (sendto(sockfd_send, rteth_buffer, sizeof(struct iphdr) + sizeof(struct tcphdr) + strlen(data) + 1, 
               0, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) 
    {
		printf("test_rteth sendto() failed\n");
				
		return -1;
    }
            
    return 0;
}

int recv_test_rteth(void)
{
     int received;
     struct udphdr * udp_header;
     struct iphdr *ip_header;
     char * data;
     
	 received = recv(sockfd_recv, rteth_buffer, sizeof(rteth_buffer), 0);
	 if (received < 0) 
	 {
		 printf("test_rteth recv() failed\n");
            
		 return -1;
	 }
	 
	 ip_header = (struct iphdr *)rteth_buffer;
			
	 if (ip_header->protocol == IPPROTO_UDP) 
	 {
		  udp_header = (struct udphdr *)(rteth_buffer + (ip_header->ihl * 4));
            
		  printf("Received UDP packet from %s:%d to %s:%d\n",
                   inet_ntoa(*(struct in_addr *)&ip_header->saddr),
                   ntohs(udp_header->source),
                   inet_ntoa(*(struct in_addr *)&ip_header->daddr),
                   ntohs(udp_header->dest));
            
		  data = rteth_buffer + (ip_header->ihl * 4) + sizeof(struct udphdr);
            
          printf("%s\n", data);
    }
    
    return 0;      
}	

int init_test_rteth(void)
{
	if ((sockfd_send = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0) 
	{
		printf("prepare_test_rteth open send_sock fail!!!\n");
		
        return -1;
    }
       
    if ((sockfd_recv = socket(AF_INET, SOCK_RAW, IPPROTO_UDP)) < 0) 
    {
		printf("prepare_test_rteth open recv_sock fail!!!\n");
		
        close(sockfd_send);
        
        sockfd_send = -1;
        
        return -1;
    }
    
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(DEST_IP);
        
    return 0;
}

void uninit_test_rteth(void)
{
	if(sockfd_send > 0)
	{
		close(sockfd_send);
		
		sockfd_send = -1;
	}
	
	if(sockfd_recv > 0)
	{
		close(sockfd_recv);
		
		sockfd_recv = -1;
	}	
}	

#define CUR_TTY_DEBUG_NAME "ttyFIQ0"

int is_tty_debug(void)
{
	char* tty_name = NULL;
	
    int stdin_is_tty = isatty(STDIN_FILENO);
    int stdout_is_tty = isatty(STDOUT_FILENO);
    int stderr_is_tty = isatty(STDERR_FILENO);
    
    if (!stdin_is_tty || !stdout_is_tty || !stderr_is_tty) {
        return 0;
    }
    
    if (stdout_is_tty) {
        tty_name = ttyname(STDOUT_FILENO);
        if (tty_name) 
        {	
			if(strstr(tty_name, CUR_TTY_DEBUG_NAME))
				return 1;
            
            return 0;
        }
        else
        {
			return 1;
		}		
    }
    
    return 0;
}	

int main(int argc , char *argv[])
{
     int i;
     int is_testrr = 0;
     int is_testbc = 0;
     int is_testk3 = 0;
     int is_testbl = 0;
     int is_testbr = 0;
     int is_test_rteth = 0;
     int is_foe_write = 0;
     int is_sii_write = 0;
     int is_foe_read = 0;
     int is_testsl = 0;
     int ret;
     int pid;
     FILE * fp = NULL;
     int argCount;
     char * erthercat_cmdname;
     int loopcnt = 0;
     int lock_file;
     uint32_t non_tty_debug;
     char * p_rtos_cmd;
     
     lock_file = open("/data/rtonboot.lock", O_CREAT | O_RDWR, 0644);
     if (lock_file == -1) 
     {
        printf("cannot create lock file");
        
        exit(EXIT_FAILURE);
     }
     
     if (flock(lock_file, LOCK_EX | LOCK_NB) == -1) 
     {
        if (errno == EWOULDBLOCK) 
        {
            printf("another rtonboot running just quit !\n");
            close(lock_file);
            return EXIT_FAILURE;
        } else {
            printf("flock fail");
            close(lock_file);
            return EXIT_FAILURE;
        }
     }
     
     if(!is_tty_debug())
     {
		 non_tty_debug = 1;
	 }
	 else
	 {
		 non_tty_debug = 0;
	 }
	 
	 p_rtos_cmd = &rtos_cmd_buf[0];
	 
	 *((uint32_t *)p_rtos_cmd) = non_tty_debug;
	 
	 p_rtos_cmd += 4;	 
   
     for(i = 1; i < argc; ++i)
     {
		 if(i == 1)
		 {
			 strcpy(p_rtos_cmd, argv[i]);
		 }
		 else
		 {
			 strcat(p_rtos_cmd, argv[i]);
		 }
		 
		 if( (i) != (argc - 1) )
		 {
			 strcat(p_rtos_cmd, " ");
		 }	  
	 }
	 
	 if(argc > 1)
	 {
		 if(!strcmp(argv[1], "testrr"))
		 {			 
			 is_testrr = 1;
		 }
		 else if(!strcmp(argv[1], "list"))
		 {
			 if(argc < 3)
			 {
				 printf("Wrong argc need pid para\n");
				 
				 ret = EXIT_FAILURE;
				 
				 goto just_unlock;
			 }
			 
			 pid = atoi(argv[2]);
			 if( (pid != -1) && (pid != -2) && (pid < 0) )
			 {
				 printf("Wrong pid, pid must be -1, -2 or positive number\n");
				 
				 ret = EXIT_FAILURE;
				 
				 goto just_unlock;
			 }
		 }
		 else if(!strcmp(argv[1], "testbc"))
		 {
			 is_testbc = 1;
		 }
		 else if(!strcmp(argv[1], "testk3"))
		 {
			 is_testk3 = 1;
		 }
		 else if(!strcmp(argv[1], "testbl"))
		 {
			 is_testbl = 1;
		 }
		 else if(!strcmp(argv[1], "testbr"))
		 {
			 is_testbr = 1;
		 }
		 else if(!strcmp(argv[1], "testsl"))
		 {
			 is_testsl = 1;
		 }
         else if(!strcmp(argv[1], "testrte"))
		 {
			 is_test_rteth = 1;
		 }	 
		 else if(!strcmp(argv[1], "ethercat"))
		 {
			 if(argc < 3)
			 {
				 printf("Wrong argc need cmd para\n");
				 
				 ret = EXIT_FAILURE;
				 
				 goto just_unlock;
			 }
			 
			 argCount = getOptions(argc - 1, &argv[1]);
			 
			 if(argCount)
			 {
				 erthercat_cmdname = argv[optind + 1];
				 
				 if(!strcmp(erthercat_cmdname, "foe_write"))
				 {
					if( argc < (optind + 3) )
					{
						printf("Wrong argc for foe_write need filename\n");
				 
						ret = EXIT_FAILURE;
				 
						goto just_unlock;
					}
				 
					is_foe_write = 1;
				}
				else if(!strcmp(erthercat_cmdname, "sii_write"))
				{
					if( argc < (optind + 3) )
					{
						printf("Wrong argc for sii_write need filename\n");
				 
						ret = EXIT_FAILURE;
				 
						goto just_unlock;
					}
				 
					is_sii_write = 1;
				}
				else if(!strcmp(erthercat_cmdname, "foe_read"))
				{
					if( argc < (optind + 3) )
					{
						printf("Wrong argc for foe_read need filename\n");
				 
						ret = EXIT_FAILURE;
				 
						goto just_unlock;
					}
				 
					is_foe_read = 1;
				}
			 }  
		 }
	 }
	 
	 if( (is_foe_write) || (is_sii_write) )
	 {
			if( (fp = fopen(argv[optind + 2], "rb")) == NULL )
			{
				printf("Failed to open %s \n", argv[3]);
				
				ret = EXIT_FAILURE;
				 
				goto just_unlock;
			}
			
			file_size = fsize(fp);
			if(!file_size)
			{
				printf("Wrong file size\n");
				
				ret = EXIT_FAILURE;
				 
				goto just_unlock;
			}
			else if( (file_size) > (2L * 1024L * 1024L))
			{
				printf("file is too large for write \n");
				
				ret = EXIT_FAILURE;
				 
				goto just_unlock;
			}
	 }
	 else if(is_foe_read)
	 {
		 if( (fp = fopen(argv[optind + 2], "wb+")) == NULL )
		 {
				printf("Failed to open %s \n", argv[3]);
				
				ret = EXIT_FAILURE;
				 
				goto just_unlock;
		 }
	 }	 
	      
     ret = RTSDKBeginInit();
     if(ret)
     {
		 ret = -1;
		 
		 goto just_uninit; 
	 }
	 
	 if(!is_test_rteth)
	 {
		ret = RTSDKCreateMsgs(cmds_msgs, 2);
		if(ret)
		{
			printf("Winfred Young create cmds msgs fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
	}	
	
	if(is_testrr)
	{
		ret = RTSDKCreateMsgs(testrr_msgs, 1);
		if(ret)
		{
			printf("Winfred Young create testrr msgs fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
	}
	else if(is_testbc)
	{
		ret = RTSDKReceiveBroadcast();
		if(ret)
		{
			printf("Winfred Young RTSDKReceiveBroadcast fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
	}
	else if(is_testk3)
	{
		ret = RTSDKCreateMsgs(testk3_msgs, 1);
		if(ret)
		{
			printf("Winfred Young create testk3 msgs fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
	}
	else if(is_testsl)
	{
		ret = RTSDKCreateMsgs(testsl_msgs, 1);
		if(ret)
		{
			printf("Winfred Young create testsl msgs fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
	}
	else if( (is_testbl) || (is_testbr) )
	{
		ret = RTSDKUseBulk();
		if(ret)
		{
			printf("Winfred Young RTSDKUseBulk fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
		
		ret = RTSDKCreateBulkMsgs(MSGID_LINUX_BULK_READY_MSG, MSGID_RTOS_BULK_READY_MSG);
		if(ret)
		{
			printf("Winfred Young RTSDKCreateBulkMsgs fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
	}
	else if(is_test_rteth)
	{
		ret = RTSDKCreateMsgs(rteth_msgs, 2);
		if(ret)
		{
			printf("Winfred Young create rteth_msgs fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
		
		if(init_test_rteth())
		{
			printf("Winfred Young init_test_rteth fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
		
		delay_us(100);
	}	
	else if(is_foe_write)
	{
		ret = RTSDKUseBulk();
		if(ret)
		{
			printf("Winfred Young RTSDKUseBulk fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
		
		ret = RTSDKCreateMsgs(foe_write_msgs, 1);
		if(ret)
		{
			printf("Winfred Young create foe_write msgs fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
	}
	else if(is_sii_write)
	{
		ret = RTSDKUseBulk();
		if(ret)
		{
			printf("Winfred Young RTSDKUseBulk fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
		
		ret = RTSDKCreateMsgs(sii_write_msgs, 1);
		if(ret)
		{
			printf("Winfred Young create foe_write msgs fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
	}
	else if(is_foe_read)
	{
		ret = RTSDKUseBulk();
		if(ret)
		{
			printf("Winfred Young RTSDKUseBulk fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
		
		ret = RTSDKCreateMsgs(foe_read_msgs, 1);
		if(ret)
		{
			printf("Winfred Young create foe_read msgs fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
	}		
	
	if((is_testk3))
	{
		ret = RTSDKSendMsg(MSGID_TESTK3);
		if(ret)
		{
			printf("Winfred Young send testk3 msgs fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
	}
	else if((is_testbl))
	{
		ret = RTSDKSendMsg(MSGID_LINUX_BULK_READY_MSG);
		if(ret)
		{
			printf("Winfred Young send linux bulk msg fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		}
	 }
	 else if(!is_test_rteth)
	 {
		 ret = RTSDKSendMsg(MSGID_EXECCMD_ON_RTOS);
		 if(ret)
		 {
			printf("Winfred Young send execcmd msg fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		 }
	 }
	 
	 if(is_foe_write)
	 {
		 fread(p_bulk, 1, file_size, fp);
		 
		 delay_us(100);
		 
		 ret = RTSDKSendMsgAndFlush(MSGID_FOE_WRITE, 1, 0, file_size);
		 if(ret)
		 {
			printf("Winfred Young send for_write msg fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		 }
	 }	 
	 else if(is_sii_write)
	 {
		 fread(p_bulk, 1, file_size, fp);
		 
		 delay_us(100);
		 
		 ret = RTSDKSendMsgAndFlush(MSGID_SII_WRITE, 1, 0, file_size);
		 if(ret)
		 {
			printf("Winfred Young send for_write msg fail\n");
		
			ret = -1;
		
			goto destroy_and_uninit;
		 }
     }
     
     loopcnt = 0;	 
	 	 
	 while (!rtonboot_shutdown)
     {
		delay_us(2);	
		 
		if(is_testbc)
		{
			if(is_testbc_msg_received)
				break;
		}
		else if(is_testbr)
		{
			if(is_testbr_msg_received)
				break;
		}
		else if(is_test_rteth)
		{
			/*ret = prepare_test_rteth_and_send(loopcnt);
			if(ret < 0)
				break;*/
				
			/*ret = recv_test_rteth();
			if(ret < 0)
				break;*/
				
			++loopcnt;
			/*if(loopcnt > MAX_RTETH_TEST_COUNT)
			{
				break;
			}*/
			
			sleep(1);	
		}	
		else if(is_foe_read)
		{
			if(is_foe_read_msg_received)
			{
				if( (foe_read_size) && (fp) )
				{
					fwrite(p_bulk, 1, foe_read_size, fp);
				}
				
				break;
			}
			else if(is_send_result_msg_received)
			{
				break;
			}	
		}			
		else if(is_testsl)
		{
			if(is_testsl_msg_received)
				break;
		}	
		else
		{
			if(is_send_result_msg_received)
				break;
		}
     };
	
destroy_and_uninit:

     RTSDKDeleteAllMsg();
	
just_uninit:     

     RTSDKUnInit();
          
     if(fp)
     {
		 fclose(fp);
	 }	
	 
	 if(is_test_rteth)
	 {
		 uninit_test_rteth();
     }	 
     
just_unlock:
     
     flock(lock_file, LOCK_UN);
     close(lock_file);
     remove("/data/rtonboot.lock"); 
     
     return ret;
}
