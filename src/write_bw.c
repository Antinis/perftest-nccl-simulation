/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005 Mellanox Technologies Ltd.  All rights reserved.
 * Copyright (c) 2009 HNR Consulting.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * $Id$
 */


#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>
#include <string.h>

#include "perftest_parameters.h"
#include "perftest_resources.h"
#include "perftest_communication.h"

enum service_t{
	MPI_SERVER,
	MPI_CLIENT
};

char** split_string(const char* str, int* count) {
    // 复制输入字符串，因为 strtok 会修改它
    char* str_copy = strdup(str);
    char* token;
    char** result = NULL;
    *count = 0;

    // 获取第一个分割的子字符串
    token = strtok(str_copy, " ");
    while (token != NULL) {
        // 重新分配内存以存储新的子字符串
        result = realloc(result, sizeof(char*) * (*count + 1));
        result[*count] = strdup(token);
        (*count)++;
        // 获取下一个分割的子字符串
        token = strtok(NULL, " ");
    }

    // 释放复制的字符串
    free(str_copy);
    return result;
}

/******************************************************************************
 ******************************************************************************/
int main(int argc, char *argv[])
{
	// 初始化MPI环境
    MPI_Init(&argc, &argv);

    // 获取进程数和当前进程的标识
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // 打印当前进程的标识
    printf("GPU15:: Hello from process %d out of %d processes\n", world_rank, world_size);

	/*
	 * 奇数world_rank进程号是client，偶数world_rank进程号是server
	 * world_rank%2是当前server序号n
	 * n号服务器接收来自n-1号服务器的数据，向n+1号发送数据
	 * n*2号进程接收来自(n-1)*2+1号（即n*2-1）进程的数据，n*2+1号进程向(n+1)*2号（即n*2+2）进程发送数据，
	*/

	int num_machines=world_size/2;
	int comm_size=atoi(argv[1]);
	FILE *file;

	// MPI_Comm new_comm;
    // int group_size = num_machines; // 只包含进程0和1
    // int ranks[16] = {0}; // 新组的进程rank
	// for(int i=0; i<num_machines; i++)
	// 	ranks[i]=i*2+1;
    // MPI_Group world_group, new_group;

    // MPI_Comm_group(MPI_COMM_WORLD, &world_group);
    // MPI_Group_incl(world_group, group_size, ranks, &new_group);
    // MPI_Comm_create(MPI_COMM_WORLD, new_group, &new_comm);

	int service_type;
	if(world_rank%2==0)
		service_type=MPI_SERVER;
	else
		service_type=MPI_CLIENT;

	if(service_type==MPI_SERVER)
	{
		char buffer[256];
		sprintf(buffer, "/home/ubuntu/perftest-4.1-0.2/ib_write_bw -d oqrdma_0 -n 8 -m 1024 -x 3 -s %d -p 1000%d --report_gbit -Q 1 -c RC", comm_size, world_rank/2);
		argv=split_string(buffer, &argc);

		char file_name[16];
		sprintf(file_name, "server_%d.log", world_rank/2);
		file=freopen(file_name, "w", stdout);
	}
	else
	{
		char buffer[256];
		sprintf(buffer, "/home/ubuntu/perftest-4.1-0.2/ib_write_bw -d oqrdma_0 -n 8 -m 1024 -x 3 -s %d -p 1000%d --report_gbit -i 1 10.110.2.%d -Q 1 -c RC", comm_size, (world_rank/2+1)%num_machines, 16);
		argv=split_string(buffer, &argc);

		char file_name[16];
		sprintf(file_name, "client_%d.log", world_rank/2);
		file=freopen(file_name, "w", stdout);

		sleep(2);
	}

	int				ret_parser,i = 0;
	struct ibv_device		*ib_dev = NULL;
	struct pingpong_context		ctx;
	struct pingpong_dest		*my_dest,*rem_dest;
	struct perftest_parameters	user_param;
	struct perftest_comm		user_comm;
	struct bw_report_data		my_bw_rep, rem_bw_rep;

	/* init default values to user's parameters */
	memset(&user_param,0,sizeof(struct perftest_parameters));
	memset(&user_comm,0,sizeof(struct perftest_comm));
	memset(&ctx,0,sizeof(struct pingpong_context));

	user_param.verb    = WRITE;
	user_param.tst     = BW;
	strncpy(user_param.version, VERSION, sizeof(user_param.version));

	/* Configure the parameters values according to user arguments or default values. */
	ret_parser = parser(&user_param,argv,argc);
	if (ret_parser) {
		if (ret_parser != VERSION_EXIT && ret_parser != HELP_EXIT)
			fprintf(stderr," Parser function exited with Error\n");
		return FAILURE;
	}

	if((user_param.connection_type == DC || user_param.use_xrc) && user_param.duplex) {
		user_param.num_of_qps *= 2;
	}

	/* Finding the IB device selected (or default if none is selected). */
	ib_dev = ctx_find_dev(user_param.ib_devname);
	if (!ib_dev) {
		fprintf(stderr," Unable to find the Infiniband/RoCE device\n");
		return FAILURE;
	}

	/* Getting the relevant context from the device */
	ctx.context = ibv_open_device(ib_dev);
	if (!ctx.context) {
		fprintf(stderr, " Couldn't get context for the device\n");
		return FAILURE;
	}

	/* See if MTU and link type are valid and supported. */
	if (check_link(ctx.context,&user_param)) {
		fprintf(stderr, " Couldn't get context for the device\n");
		return FAILURE;
	}

	/* copy the relevant user parameters to the comm struct + creating rdma_cm resources. */
	if (create_comm_struct(&user_comm,&user_param)) {
		fprintf(stderr," Unable to create RDMA_CM resources\n");
		return FAILURE;
	}

	if (user_param.output == FULL_VERBOSITY && user_param.machine == SERVER) {
		printf("\n************************************\n");
		printf("* Waiting for client to connect... *\n");
		printf("************************************\n");
	}

	/* Initialize the connection and print the local data. */
	if (establish_connection(&user_comm)) {
		fprintf(stderr," Unable to init the socket connection\n");
		return FAILURE;
	}
	sleep(1);
	exchange_versions(&user_comm, &user_param);

	check_sys_data(&user_comm, &user_param);

	/* See if MTU and link type are valid and supported. */
	if (check_mtu(ctx.context,&user_param, &user_comm)) {
		fprintf(stderr, " Couldn't get context for the device\n");
		return FAILURE;
	}

	ALLOCATE(my_dest , struct pingpong_dest , user_param.num_of_qps);
	memset(my_dest, 0, sizeof(struct pingpong_dest)*user_param.num_of_qps);
	ALLOCATE(rem_dest , struct pingpong_dest , user_param.num_of_qps);
	memset(rem_dest, 0, sizeof(struct pingpong_dest)*user_param.num_of_qps);

	/* Allocating arrays needed for the test. */
	alloc_ctx(&ctx,&user_param);

	/* Create (if necessary) the rdma_cm ids and channel. */
	if (user_param.work_rdma_cm == ON) {

		if (user_param.machine == CLIENT) {
			if (retry_rdma_connect(&ctx,&user_param)) {
				fprintf(stderr,"Unable to perform rdma_client function\n");
				return FAILURE;
			}

		} else {
			if (create_rdma_resources(&ctx,&user_param)) {
				fprintf(stderr," Unable to create the rdma_resources\n");
				return FAILURE;
			}
			if (rdma_server_connect(&ctx,&user_param)) {
				fprintf(stderr,"Unable to perform rdma_client function\n");
				return FAILURE;
			}
		}

	} else {

		/* create all the basic IB resources (data buffer, PD, MR, CQ and events channel) */
		if (ctx_init(&ctx, &user_param)) {
			fprintf(stderr, " Couldn't create IB resources\n");
			return FAILURE;
		}
	}

	/* Set up the Connection. */
	if (set_up_connection(&ctx,&user_param,my_dest)) {
		fprintf(stderr," Unable to set up socket connection\n");
		return FAILURE;
	}

	/* Print basic test information. */
	ctx_print_test_info(&user_param);


	/* Print this machine QP information */
	for (i=0; i < user_param.num_of_qps; i++)
		ctx_print_pingpong_data(&my_dest[i],&user_comm);


	user_comm.rdma_params->side = REMOTE;
	for (i=0; i < user_param.num_of_qps; i++) {

		if (ctx_hand_shake(&user_comm,&my_dest[i],&rem_dest[i])) {
			fprintf(stderr," Failed to exchange data between server and clients\n");
			return FAILURE;
		}

		ctx_print_pingpong_data(&rem_dest[i],&user_comm);
	}

	if (user_param.work_rdma_cm == OFF) {
		if (ctx_check_gid_compatibility(&my_dest[0], &rem_dest[0])) {
			fprintf(stderr,"\n Found Incompatibility issue with GID types.\n");
			fprintf(stderr," Please Try to use a different IP version.\n\n");
			return FAILURE;
		}
	}

	if (user_param.work_rdma_cm == OFF) {
		if (ctx_connect(&ctx,rem_dest,&user_param,my_dest)) {
			fprintf(stderr," Unable to Connect the HCA's through the link\n");
			return FAILURE;
		}
	}

	/* An additional handshake is required after moving qp to RTR. */
	if (ctx_hand_shake(&user_comm,&my_dest[0],&rem_dest[0])) {
		fprintf(stderr," Failed to exchange data between server and clients\n");
		return FAILURE;
	}

	if (user_param.output == FULL_VERBOSITY) {
		if (user_param.report_per_port) {
			printf(RESULT_LINE_PER_PORT);
			printf((user_param.report_fmt == MBS ? RESULT_FMT_PER_PORT : RESULT_FMT_G_PER_PORT));
		}
		else {
			printf(RESULT_LINE);
			printf((user_param.report_fmt == MBS ? RESULT_FMT : RESULT_FMT_G));
		}

		printf((user_param.cpu_util_data.enable ? RESULT_EXT_CPU_UTIL : RESULT_EXT));
	}

	/* For half duplex tests, server just waits for client to exit */
	if (user_param.machine == SERVER && !user_param.duplex) {

		if (ctx_hand_shake(&user_comm,&my_dest[0],&rem_dest[0])) {
			fprintf(stderr," Failed to exchange data between server and clients\n");
			return FAILURE;
		}

		// printf("Server waiting\n");
		// fflush(stdout);
		// MPI_Barrier(MPI_COMM_WORLD);

		xchg_bw_reports(&user_comm, &my_bw_rep,&rem_bw_rep,atof(user_param.rem_version));
		print_full_bw_report(&user_param, &rem_bw_rep, NULL);

		if (ctx_close_connection(&user_comm,&my_dest[0],&rem_dest[0])) {
			fprintf(stderr,"Failed to close connection between server and client\n");
			return FAILURE;
		}

		if (user_param.output == FULL_VERBOSITY) {
			if (user_param.report_per_port)
				printf(RESULT_LINE_PER_PORT);
			else
				printf(RESULT_LINE);
		}

		if (user_param.work_rdma_cm == ON) {
			if (destroy_ctx(&ctx,&user_param)) {
				fprintf(stderr, "Failed to destroy resources\n");
				return FAILURE;
			}
			user_comm.rdma_params->work_rdma_cm = ON;
			return destroy_ctx(user_comm.rdma_ctx,user_comm.rdma_params);
		}

		return destroy_ctx(&ctx,&user_param);
	}

	if (user_param.test_method == RUN_ALL) {

		for (i = 1; i < 24 ; ++i) {

			user_param.size = (uint64_t)1 << i;
			ctx_set_send_wqes(&ctx,&user_param,rem_dest);

			if (user_param.perform_warm_up) {
				if(perform_warm_up(&ctx, &user_param)) {
					fprintf(stderr, "Problems with warm up\n");
					return FAILURE;
				}
			}

			if(user_param.duplex) {
				if (ctx_hand_shake(&user_comm,&my_dest[0],&rem_dest[0])) {
					fprintf(stderr,"Failed to sync between server and client between different msg sizes\n");
					return FAILURE;
				}
			}

			if(run_iter_bw(&ctx,&user_param)) {
				fprintf(stderr," Failed to complete run_iter_bw function successfully\n");
				return FAILURE;
			}

			if (user_param.duplex && (atof(user_param.version) >= 4.6)) {
				if (ctx_hand_shake(&user_comm,&my_dest[0],&rem_dest[0])) {
					fprintf(stderr,"Failed to sync between server and client between different msg sizes\n");
					return FAILURE;
				}
			}

			print_report_bw(&user_param,&my_bw_rep);

			if (user_param.duplex) {
				xchg_bw_reports(&user_comm, &my_bw_rep,&rem_bw_rep,atof(user_param.rem_version));
				print_full_bw_report(&user_param, &my_bw_rep, &rem_bw_rep);
			}
		}

	} else if (user_param.test_method == RUN_REGULAR) {

		// printf("Client waiting\n");
		// fflush(stdout);
		// MPI_Barrier(new_comm);

		/*
		 * 想要开始第i轮发送，必须完成了第i-1轮的接收。
		 * 接收轮次可以超前于发送，但发送无法超前于接收。
		 * n号机器在进行第i次发送之前，阻塞式接收n-1号机器发来的第i-1次发送的结束信号，代表完成第i-1次接收。
		*/
		for(int i=0; i<2*num_machines; i++)
		{	
			// printf("GPU15:: entry\n");
			if(i!=0)
			{
				int flag;
				// printf("rank %d recv from rank %d\n", world_rank, (world_rank+world_size-2)%world_size);
				// fflush(stdout);
				MPI_Recv(&flag, 1, MPI_INT, (world_rank+world_size-2)%world_size, i-1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				// printf("rank %d finished recv from rank %d\n", world_rank, (world_rank+world_size-2)%world_size);
				// fflush(stdout);
			}

			ctx_set_send_wqes(&ctx,&user_param,rem_dest);

			// printf("finished wqe\n");
			// fflush(stdout);
			
			if (user_param.verb != SEND) {
				
				if (user_param.perform_warm_up) {
					if(perform_warm_up(&ctx, &user_param)) {
						fprintf(stderr, "Problems with warm up\n");
						return FAILURE;
					}
				}
			}

			if(user_param.duplex) {
				if (ctx_hand_shake(&user_comm,&my_dest[0],&rem_dest[0])) {
					fprintf(stderr,"Failed to sync between server and client between different msg sizes\n");
					return FAILURE;
				}
			}

			if(run_iter_bw(&ctx,&user_param)) {
				fprintf(stderr," Failed to complete run_iter_bw function successfully\n");
				return FAILURE;
			}

			// 通知n+1号机器本轮发送结束
			int flag=1;
			MPI_Request request;
			// printf("rank %d send to rank %d\n", world_rank, (world_rank+2)%world_size);
			MPI_Isend(&flag, 1, MPI_INT, (world_rank+2)%world_size, i, MPI_COMM_WORLD, &request);
			// printf("rank %d finished send to rank %d\n", world_rank, (world_rank+2)%world_size);

			print_report_bw(&user_param,&my_bw_rep);

			// printf("GPU15:: exit\n");
			// fflush(stdout);

			if (user_param.duplex) {
				xchg_bw_reports(&user_comm, &my_bw_rep,&rem_bw_rep,atof(user_param.rem_version));
				print_full_bw_report(&user_param, &my_bw_rep, &rem_bw_rep);
			}

			if (user_param.report_both && user_param.duplex) {
				printf(RESULT_LINE);
				printf("\n Local results: \n");
				printf(RESULT_LINE);
				printf((user_param.report_fmt == MBS ? RESULT_FMT : RESULT_FMT_G));
				printf((user_param.cpu_util_data.enable ? RESULT_EXT_CPU_UTIL : RESULT_EXT));
				print_full_bw_report(&user_param, &my_bw_rep, NULL);
				printf(RESULT_LINE);

				printf("\n Remote results: \n");
				printf(RESULT_LINE);
				printf((user_param.report_fmt == MBS ? RESULT_FMT : RESULT_FMT_G));
				printf((user_param.cpu_util_data.enable ? RESULT_EXT_CPU_UTIL : RESULT_EXT));
				print_full_bw_report(&user_param, &rem_bw_rep, NULL);
			}
		}
	} else if (user_param.test_method == RUN_INFINITELY) {

		ctx_set_send_wqes(&ctx,&user_param,rem_dest);

		if(run_iter_bw_infinitely(&ctx,&user_param)) {
			fprintf(stderr," Error occured while running infinitely! aborting ...\n");
			return FAILURE;
		}
	}

	if (user_param.output == FULL_VERBOSITY) {
		if (user_param.report_per_port)
			printf(RESULT_LINE_PER_PORT);
		else
			printf(RESULT_LINE);
	}

	/* For half duplex tests, server just waits for client to exit */
	if (user_param.machine == CLIENT && !user_param.duplex) {

		if (ctx_hand_shake(&user_comm,&my_dest[0],&rem_dest[0])) {
			fprintf(stderr," Failed to exchange data between server and clients\n");
			return FAILURE;
		}

		xchg_bw_reports(&user_comm, &my_bw_rep,&rem_bw_rep,atof(user_param.rem_version));
	}

	/* Closing connection. */
	if (ctx_close_connection(&user_comm,&my_dest[0],&rem_dest[0])) {
		fprintf(stderr,"Failed to close connection between server and client\n");
		return FAILURE;
	}

	if (!user_param.is_bw_limit_passed && (user_param.is_limit_bw == ON ) ) {
		fprintf(stderr,"Error: BW result is below bw limit\n");
		return FAILURE;
	}

	if (!user_param.is_msgrate_limit_passed && (user_param.is_limit_bw == ON )) {
		fprintf(stderr,"Error: Msg rate  is below msg_rate limit\n");
		return FAILURE;
	}

	free(my_dest);
	free(rem_dest);

	if (user_param.work_rdma_cm == ON) {
		if (destroy_ctx(&ctx,&user_param)) {
			fprintf(stderr, "Failed to destroy resources\n");
			return FAILURE;
		}
		user_comm.rdma_params->work_rdma_cm = ON;
		return destroy_ctx(user_comm.rdma_ctx,user_comm.rdma_params);
	}

	fclose(file);

	// 终止MPI环境
    MPI_Finalize();

	return destroy_ctx(&ctx,&user_param);
}
