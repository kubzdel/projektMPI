#include "mpi.h"
#include <stdio.h>
#include<pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <queue>
#include <functional>
#define P 5
#define J 20
#define L 10
#define MSG_SIZE 1

/*
	status
		transport do szpitala: 0
		szpital: 1
		transport ze szpitala: 2


*/

using namespace std;

int status;
int localClock;

	int rank11 = -1;
struct sectionRequest{
    int clock;
    int section;
};

struct sectionResponse{
    int status;
    int section;
};

void *communicationThread(void *)
{
	//while(1)
	//{
	int request = rank11;
    int sender = (rank11 - 1 == 0) ? 0 : 1;
	if(rank11 == request)
	{
		MPI_Bcast(&request, MSG_SIZE, MPI_INT, sender, MPI_COMM_WORLD);
	
		printf("receiving %d %d\n",rank11, sender);
	}
	
	//}
}
void initParallelThread()
{
	pthread_t pass_thread;
	pthread_create(&pass_thread, NULL, communicationThread, NULL);
}

bool compare(sectionRequest a,sectionRequest b)
{
   return (a.clock<b.clock);
}


int random(int min, int max)
{
    int tmp;
    if (max>=min)
        max-= min;
    else
    {
        tmp= min - max;
        min= max;
        max= tmp;
    }
    return max ? (rand() % max + min) : min;
}
int main( int argc, char **argv )
{
std::priority_queue<sectionRequest, std::vector<sectionRequest>,
                              std::function<bool(sectionRequest, sectionRequest)>> pq(compare);
	MPI_Init(&argc, &argv);
	initParallelThread();
	MPI_Status status1;

	MPI_Comm_rank( MPI_COMM_WORLD, &rank11 );
    printf("My rank is %d\n", rank11);
	status=0;
	sleep(1);
	sectionRequest hospitalPlace;
	hospitalPlace.clock = localClock;
	hospitalPlace.section = 1;
	int sender = 1; //pierwszy watek wysyla
	if(sender == rank11)
	{
		MPI_Bcast(&hospitalPlace.clock, MSG_SIZE, MPI_INT, sender, MPI_COMM_WORLD);
		printf("sending to all %d\n", hospitalPlace.clock);
	}
		
	status=1;
	sleep(10);
	MPI_Finalize();
	
}
