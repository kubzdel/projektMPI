#include "mpi.h"
#include <stdio.h>
#include<pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <queue>
#include <functional>
#include<list>
#define P 5
#define J 20
#define L 10
#define MSG_SIZE 2
#define MSG_HELLO 100
#define MSG_REQUEST 200
#define MSG_RESPONSE 300
#define MSG_RELEASE 400
/*
	status
		transport do szpitala: 0
		szpital: 1
		transport ze szpitala: 2


*/

using namespace std;


int status;
int localClock=0;
int world_size; 
	int rank11 = -1;
struct sectionRequest{
    int clock;
    int section;
	int id;
	bool operator <(const sectionRequest & playerObj) const
	{
		return clock < playerObj.clock;
	}
};

struct sectionResponse{
    int status;
    int section;
};


bool compare(sectionRequest a,sectionRequest b)
{
   return (a.clock<b.clock);
}

std::priority_queue<sectionRequest, std::vector<sectionRequest>,
                              std::function<bool(sectionRequest, sectionRequest)>> hospitalQueue(compare);

std::priority_queue<sectionRequest, std::vector<sectionRequest>,
                              std::function<bool(sectionRequest, sectionRequest)>> teleporterQueue(compare);

std::list<sectionRequest> hospitalList;  
std::list<sectionRequest> teleporterList;  
void *communicationThread(void *)
{
	//while(1)
	//{
	// int request = rank11;
    // int sender = (rank11 - 1 == 0) ? 0 : 1;
	// if(rank11 == request)
	// {
	// 	MPI_Bcast(&request, MSG_SIZE, MPI_INT, sender, MPI_COMM_WORLD);
	
	// 	printf("receiving %d %d\n",rank11, sender);
	// }
	while(1)
	{
		int msg[MSG_SIZE];
		MPI_Status status;
		MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MSG_REQUEST, MPI_COMM_WORLD, &status);
		printf("Wątek %d otrzymał żądanie od wątku %d \n",rank11,status.MPI_SOURCE);
		int messageType = status.MPI_TAG;

		if(messageType == MSG_REQUEST)
		{
			sectionRequest request;
			request.clock = msg[0];
			request.section = msg[1];
			request.id = status.MPI_SOURCE;

			if(request.section == 1)
			{
				//hospitalQueue.push(request);
				hospitalList.push_back(request);
				hospitalList.sort();
			}

			else
			{
				teleporterList.push_back(request);
				teleporterList.sort();
			}

			MPI_Send( msg, MSG_SIZE, MPI_INT, status.MPI_SOURCE, MSG_RESPONSE, MPI_COMM_WORLD );
			printf("Wątek %d odesłał potwierdzenie do wątku %d \n",rank11,status.MPI_SOURCE);
		}
		else if(messageType == MSG_RELEASE)
		{
			
		}
	}
	//printf("otrzymany zegar %d \n",msg[0]);
	
	//}
}
void initParallelThread()
{
	pthread_t pass_thread;
	pthread_create(&pass_thread, NULL, communicationThread, NULL);
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

	MPI_Init(&argc, &argv);
	initParallelThread();
	MPI_Status status1;

	MPI_Comm_rank( MPI_COMM_WORLD, &rank11 );
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    printf("My rank is %d\n", rank11);
	status=0;
	sleep(1);
	localClock+=1;
	sectionRequest hospitalPlace;
	hospitalPlace.clock = localClock;
	hospitalPlace.section = 1;
	int msg[] = {hospitalPlace.clock,hospitalPlace.section};
	for(int i=0;i<world_size;i++)
	{
		if(i!=rank11)
		{
			MPI_Send( msg, MSG_SIZE, MPI_INT, i, MSG_REQUEST, MPI_COMM_WORLD );
			printf("Wątek %d wysłał żądanie do wątku %d  \n",rank11,i);
		}
	}

	for(int i=0;i<world_size-1;i++)
	{
		MPI_Status status;
		MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MSG_RESPONSE, MPI_COMM_WORLD, &status);
		printf("Wątek %d otrzymał %d potwierdzenie od wątku %d \n",rank11,i+1,status.MPI_SOURCE);
	}
		
	printf("Wątek %d zakończył pracę \n",rank11);
	
	status=1;
	sleep(100);
	MPI_Finalize();
	
}
