/*
 * RDMAParser.cpp
 *
 *  Created on: 2015-5-14
 *      Author: hek
 */

#include "RDMAParser.h"

uint32_t DebugLevel = 0;

RDMAParser::RDMAParser() {
	// TODO 自动生成的构造函数存根

}

RDMAParser::~RDMAParser() {
	// TODO 自动生成的析构函数存根
}

bool RDMAParser::Parse(struct user_param_t *user_param, char *argv[], int argc)
{
	int c;

	initUserParam(user_param);

	int index = 0;
	struct option options[18];
	options[index].name = "help";				options[index].has_arg = 0;		options[index].val = 'h';	index++;
	options[index].name = "ip";					options[index].has_arg = 1;		options[index].val = 'a';	index++;
	options[index].name = "ib_dev";				options[index].has_arg = 1;		options[index].val = 'd';	index++;
	options[index].name = "direction";			options[index].has_arg = 1;		options[index].val = 'D';	index++;
	options[index].name = "event";				options[index].has_arg = 0;		options[index].val = 'e';	index++;
	options[index].name = "debug_level";		options[index].has_arg = 1;		options[index].val = 'G';	index++;
	options[index].name = "opcode";				options[index].has_arg = 1;		options[index].val = 'o';	index++;
	options[index].name = "num_of_outs";		options[index].has_arg = 1;		options[index].val = 'O';	index++;
	options[index].name = "server_port";		options[index].has_arg = 1;		options[index].val = 'p';	index++;
	options[index].name = "num_of_thread";		options[index].has_arg = 1;		options[index].val = 'T';	index++;
	options[index].name = "src_file_path";		options[index].has_arg = 1;		options[index].val = 'f';	index++;
	options[index].name = "target_file_path";	options[index].has_arg = 1;		options[index].val = 't';	index++;
	options[index].name = "resume_trans";		options[index].has_arg = 0;		options[index].val = 'r';	index++;
	options[index].name = "trans_msg_file";		options[index].has_arg = 0;		options[index].val = 'm';	index++;

	while (1)
	{
		c = getopt_long(argc, argv, "a:d:D:G:o:O:p:T:f:t:herm", options, NULL);
		if (c == -1)
		{
			break;
		}

		switch (c)
		{
		case 'h':
			usage(argv[0]);
			return false;
		case 'a':
			user_param->ip = strdup(optarg);
			break;
		case 'd':
			user_param->hca_id = strdup(optarg);
			break;
		case 'D':
			user_param->direction = strtol(optarg, NULL, 0);
			break;
		case 'e':
			user_param->use_event = 1;
			break;
		case 'G':
			user_param->debug_level = strtol(optarg, NULL, 0);
			break;
		case 'o':
			user_param->opcode = (ibv_wr_opcode)strtol(optarg, NULL, 0);
			break;
		case 'O':
			user_param->num_of_oust = strtol(optarg, NULL, 0);
			break;
		case 'p':
			user_param->tcp_port = strtol(optarg, NULL, 0);
			break;
		case 'T':
			user_param->num_of_thread = strtol(optarg, NULL, 0);
			break;
		case 'f':
			user_param->src_file_path = strdup(optarg);
			break;
		case 't':
			user_param->target_file_path = strdup(optarg);
			break;
		case 'r':
			user_param->is_resume_trans = 1;
			break;
		case 'm':
			user_param->is_trans_msg_file = 1;
			break;
		default:
			printf("非法命令或参数.\n");
			usage(argv[0]);
			return false;
		}
	}

	DebugLevel = user_param->debug_level;
	if (optind == argc - 1)
	{
		user_param->server_ip = strdup(argv[optind]);
	}
	else if (optind < (argc - 1))
	{
		printf(" 命令行错误，请检查命令后重新运行.\n");
		return false;
	}

	if (user_param->server_ip && (user_param->opcode != IBV_WR_SEND))
	{
		printf("非法操作码，当前仅支持SEND操作.\n");
		usage(argv[0]);
		return false;
	}

	return true;
}

void RDMAParser::initUserParam(struct user_param_t *user_param)
{
	user_param->server_ip = NULL;
	user_param->hca_id = (char*)malloc(256);
	strcpy(user_param->hca_id, "mlx4_0");
	user_param->ip = "127.0.0.1";
	user_param->tcp_port = 18752;
	user_param->num_of_thread = 1;
	user_param->num_of_oust = 1024;			// 开始一次性Post多少个接收请求，以及max_send_wr和max_recv_wr数
	user_param->direction = 0;
	user_param->use_event = 0;
	user_param->debug_level = 2;
	user_param->opcode = IBV_WR_SEND;
	user_param->src_file_path = (char*)malloc(BUFSIZ);
	strcpy(user_param->src_file_path, "/home/hek/clips/download.ts");
	user_param->is_resume_trans = 0;
	user_param->is_trans_msg_file = 0;
	user_param->target_file_path = (char*)malloc(BUFSIZ);
	memset(user_param->target_file_path, 0, BUFSIZ);
}

void RDMAParser::usage(const char *argv0)
{
	printf("Usage:\n");
	printf(
			"  %s                      start a server and wait for connection\n",
			argv0);
	printf("  %s [Options] <host>     connect to server at <host>\n", argv0);
	printf("\n");
	printf("Options:\n");

	printf("  -h, --help");
	printf("    Show this help screen.\n");

	printf("  -a, --ip=<ip address>");
	printf("    IP address of server.\n");

	printf("  -d, --ib_dev=<dev>");
	printf("    Use IB device <dev> (default first device found)\n");

	printf("  -D, --direction=<direction>");
	printf("    Direction of RDMA transaction, 0: BI, 1: Client -> Server, 2: Server -> Client.\n");

	printf("  -e, --events");
	printf("    Sleep on CQ events (default poll)\n");
	
	printf("  -G, --debug_level=<debug_level>, 0: NONE, >=1: ERROR, >=2: INFO, >=3: TRACE, >=4: DEBUG\n");

	printf("  -o, --opcode=<opcode>");
	printf("    RDMA Operation Code, 0: RDMA_WRITE, 1: RDMA_WRITE_IMM, 2: SEND, 3: SEND_IMM, 4: RDMA_READ, 5: CAS, 6: FAA (only 2: SEND now).\n");

	printf("  -p, --port=<port>");
	printf("    TCP port of server.\n");

	printf("  -T, --num_of_thread=<num_of_thread>");
	printf("    The number of threads which will be created.\n");

	printf("  -f, --file_path=<file path to transfer>");
	printf("	Transfer local file path.\n");

	printf("  -t, --target_file_path=<target file path to transfer>");
	printf("	Transfer target file path.\n");

	printf("  -r, --resume_trans");
	printf("	whether resume transfer file (default false).\n");

	printf("  -m, --trans_msg_file");
	printf("	whether transfer message file path (default false).\n");

	printf("Examples:\n");
	printf("    Server:.\n");
	printf("        ./IBDataTransfer -G 2 -T 3 \n");
	printf("    Client:.\n");
	printf("        ./IBDataTransfer -G 2 -r -f /home/hek/clips/test_2.ts 192.168.1.11 \n");
	printf("		./IBDataTransfer -G 2 -m -f /tmp/ibmsg/msg.xml 192.168.1.11 \n");

	putchar('\n');
}

void RDMAParser::dumpArgs(struct rdma_resource_t *rdma_resource)
{
	struct user_param_t *user_param = &(rdma_resource->user_param);

	DEBUG("Input parameters after sync:\n");
	DEBUG("\tnum_of_thread:%d\n", user_param->num_of_thread);
	DEBUG("\tqp_num       :%d\n", user_param->num_of_qp);
	DEBUG("\tcq_num       :%d\n", user_param->num_of_cq);
	DEBUG("\tsrq_num      :%d\n", user_param->num_of_srq);
	DEBUG("\topcode       :%d\n", user_param->opcode);
}








