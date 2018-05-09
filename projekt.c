#include "mpi.h"
#include <stdio.h>
#include<pthread.h>
#include <sys/types.h>
#include <unistd.h>
#define P 5
#define J 20
#define L 10
int status;
queue *pocz,*s;
struct queue
{
    int id;
    queue *next;
};

void push(int id)
{
if(pocz==NULL)
	{
		s=malloc(siezof (struct queue));
		s->id=id;
		s->next=NULL;
		pocz=s;
	}
else
	{
		s=malloc(siezof (struct queue));
		s->id=id;
		s->next=pocz;
		pocz=s;
	}
}

int pop()
{
int item;
if(pocz!=NULL)
	{
		s=pocz;
		item=pocz->id;
		pocz=s->next;
		
		

		


struct sectionRequest{
    int clock;
    int section;
}

struct sectionResponse{
    int status;
    int section;
}

void initParallelThread()
{
	pthread_t pass_thread;
	pthread_create(&pass_thread, NULL, communicationThread, NULL);
}

void communicationThread()
{
	while(1)
	{
	MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
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
	MPI_Init(&argc, &argv);
	status=0;
	sleep(1,10);
	status=1;
	











	MPI_Finalize();
	
