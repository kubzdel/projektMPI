//#include "mpi.h"
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


struct sectionRequest{
    int clock;
    int section;
};

struct sectionResponse{
    int status;
    int section;
};
void communicationThread();
void initParallelThread()
{
	pthread_t pass_thread;
	//pthread_create(&pass_thread, NULL, communicationThread, NULL);
}

bool compare(sectionRequest a,sectionRequest b)
{
   return (a.clock<b.clock);
}

void communicationThread()
{
	while(1)
	{
	//MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	}
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
	//MPI_Init(&argc, &argv);
	status=0;
	//sleep(1,10);
	status=1;
		
	
	//MPI_Finalize();
	
}
