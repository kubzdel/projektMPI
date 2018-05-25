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

/*
	status
		transport do szpitala: 0
		szpital: 1
		transport ze szpitala: 2


*/

using namespace std;






int status;
int localClock;

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
	sectionRequest request;
	MPI_Bcast(&request, sizeof(struct sectionRequest), MPI_INT, 0, MPI_COMM_WORLD);
	printf("%d",request.clock);
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
	
	int rank;
	MPI_Comm_rank( MPI_COMM_WORLD, &rank );
	status=0;
	sleep(1);
	sectionRequest hospitalPlace;
	hospitalPlace.clock = localClock;
	hospitalPlace.section = 1;
	MPI_Bcast(&hospitalPlace, sizeof(struct sectionRequest), MPI_INT, 0, MPI_COMM_WORLD);
	status=1;
	sleep(10);
	MPI_Finalize();
	
}
