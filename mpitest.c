#include <mpi.h>
#include <stdio.h>

int main(int argc, char** argv) {
    // 初始化MPI环境
    MPI_Init(&argc, &argv);

    // 获取总的进程数
    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    // 获取当前进程的ID（rank）
    int world_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // 获取处理器名称
    char processor_name[MPI_MAX_PROCESSOR_NAME];
    int name_len;
    MPI_Get_processor_name(processor_name, &name_len);

    // 打印信息
    printf("Hello world from processor %s, rank %d out of %d processors\n",
           processor_name, world_rank, world_size);

    

    for(int i=0; i<4; i++)
    {	
        printf("GPU15:: entry\n");
        if(i!=0)
        {
            int flag;
            printf("rank %d recv from rank %d\n", world_rank, (world_rank+world_size-2)%world_size);
            fflush(stdout);
            MPI_Recv(&flag, 1, MPI_INT, (world_rank+world_size-2)%world_size, i-1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            printf("rank %d finished recv from rank %d\n", world_rank, (world_rank+world_size-2)%world_size);
            fflush(stdout);
        }

        printf("finished wqe\n");
        fflush(stdout);
        
       

        // 通知n+1号机器本轮发送结束
        int flag=1;
        MPI_Request request;
        printf("rank %d send to rank %d\n", world_rank, (world_rank+2)%world_size);
        MPI_Isend(&flag, 1, MPI_INT, (world_rank+2)%world_size, i, MPI_COMM_WORLD, &request);
        printf("rank %d finished send to rank %d\n", world_rank, (world_rank+2)%world_size);


        printf("GPU15:: exit\n");
        fflush(stdout);
    }



    // 终止MPI环境
    MPI_Finalize();

    return 0;
}