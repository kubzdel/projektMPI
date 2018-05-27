#include "mpi.h"
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <iostream>
#include <queue>
#include <functional>
#include <list>
#include <mutex>
#include <sys/types.h> /* for open(2) */
#include <sys/stat.h> /* for open(2) */
#include <fcntl.h> /* for open(2) */
#include <unistd.h> /* for read(2), close(2) */
#include <signal.h>

#define P 5
#define J 20
#define L 10
#define MSG_SIZE 2
#define MSG_HELLO 100
#define MSG_REQUEST 200
#define MSG_RESPONSE 300
#define MSG_RELEASE 400

//przy konwersji na msg
#define SECTION 0
#define ID 1

#define DEVURANDOM "/dev/urandom"

//do release zeby wiedziec ktorej sekcji on dotyczy
#define HOSPITAL_STATUS 0
#define TELEPORTER_STATUS 1
/*
	status
		teleporter: 0
		szpital: 1
*/

using namespace std;

int status;
int localClock=0;
int world_size; 
int processID = -1;

int random_int()
{
    int rnum = 0;
    int fd = open(DEVURANDOM, O_RDONLY);
    if (fd != -1)
    {
        (void) read(fd, (void *)&rnum, sizeof(int));
        (void) close(fd);
    }
    return rnum;
}

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

void signalHandler(int dummy)
{
	cout << "Signal " << dummy << " received in process " << processID << " pid: " << getpid() << endl;
	MPI_Finalize();
	exit(0);
}


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
mutex hospital_mutex;
mutex transport_mutex;

void *communicationThread(void *)
{
	cout << "Communication thread for process " << processID << " has PID " << getpid() << endl;
	//mpi sie uruchamia wolniej niz tworzy sie watek wiec trzeba sprawdzac czy processID jest rozny od -1
	while(1)
	{
		int msg[MSG_SIZE];
		MPI_Status status;
		MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MSG_REQUEST, MPI_COMM_WORLD, &status);
		
		//mamy jeden Recv do request i release wiec i tak zawsze odbieramy tablice, chociaz przy release wystraczy jedna liczba
		sectionRequest request;
		request.clock = msg[SECTION];
		request.section = msg[ID];
		request.id = status.MPI_SOURCE;
		int id = request.id;
		printf("Wątek %d otrzymał żądanie od wątku %d \n",processID,status.MPI_SOURCE);
		int messageType = status.MPI_TAG;

		if(messageType == MSG_REQUEST)
		{
			if(request.section == 1)
			{
				hospital_mutex.lock();
				hospitalList.push_back(request);
				hospitalList.sort();
				hospital_mutex.unlock();
			}

			else
			{
				transport_mutex.lock();
				teleporterList.push_back(request);
				teleporterList.sort();
				transport_mutex.unlock();
			}

			MPI_Send( msg, MSG_SIZE, MPI_INT, status.MPI_SOURCE, MSG_RESPONSE, MPI_COMM_WORLD );
			printf("Wątek %d odesłał potwierdzenie do wątku %d \n",processID,status.MPI_SOURCE);
		}
		else if(messageType == MSG_RELEASE)
		{
			cout << processID <<": Usuwam proces nr " << request.id << " z kolejki " << (request.section == 0 ? "teleportera" : "szpitala") << endl;
			//usun goscia z kolejki
			switch(request.section)
			{
				case HOSPITAL_STATUS:
					hospitalList.remove_if([&request](sectionRequest item){ return item.id == request.id;});
					break;
				case TELEPORTER_STATUS: 
					teleporterList.remove_if([&request](sectionRequest item){ return item.id == request.id;});
					break;
				default: break;
			}
		}
	}
}

pthread_t initParallelThread()
{
	pthread_t newThread;
	pthread_create(&newThread, NULL, communicationThread, NULL);
	return newThread;
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

//do wyswietlania elementów kolejki
std::ostream& operator<<(std::ostream& ostr, const std::list<sectionRequest>& list)
{
    for (auto &i : list) {
        ostr <<  "\t mojeID=" << processID << "\tproc = " << i.id << " zegar=" << i.clock << endl;
    }
    return ostr;
}

int main( int argc, char **argv )
{
	signal(SIGSEGV, signalHandler);
	MPI_Init(&argc, &argv);
	cout << "Main thread for process " << processID << " has PID " << getpid() << endl;

	pthread_t communicationThread = initParallelThread();
	MPI_Comm_rank( MPI_COMM_WORLD, &processID );
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	status=0;
	srand(random_int());
	int waitTime = random(5, 25);
	printf("Proces z id = %d, czekanie %.2f sek. na rozpoczęcie.\n", processID, waitTime / 10.f);
	usleep(waitTime*100 * 1000); //0.5s-2.5s
	while(true){
		localClock+=1;
		sectionRequest hospitalPlace;
		hospitalPlace.clock = localClock;
		hospitalPlace.section = 1;
		int msg[] = {hospitalPlace.clock,hospitalPlace.section};
		for(int i=0;i<world_size;i++)
		{
			if(i!=processID)
			{
				MPI_Send( msg, MSG_SIZE, MPI_INT, i, MSG_REQUEST, MPI_COMM_WORLD );
				printf("Wątek %d wysłał żądanie do wątku %d  \n",processID,i);
			}
		}

		for(int i=0;i<world_size-1;i++)
		{
			MPI_Status status;
			MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MSG_RESPONSE, MPI_COMM_WORLD, &status);
			printf("Wątek %d otrzymał potwierdzenie nr %d, od wątku %d \n",processID,i+1,status.MPI_SOURCE);
		}

		hospital_mutex.lock();	//zeby nie pobrac danej przed dodaniem lub podczas sortowania
		sectionRequest first_on_list = hospitalList.front(); 
		hospital_mutex.unlock();
		//w tym momencie otrzymanie żądania od innego procesu nie powinno zmienić
		if(first_on_list.id == processID)
		{
			int waitTime = random(10,20);
			cout << "--------------------------->Wątek " << processID << ", czeka " << waitTime << " sek. w SEKCJI KRYTYCZNEJ HOSPITAL\n";
			sleep(waitTime);
			//usun swoje żądanie z sekcji krytycznej
			//u reszty procesów
			for(int i=0;i<world_size;i++)
			{
				if(i!=processID)
				{
					//tu troche bezsensu wysylamy az dwa inty ale pozniej w odbieraniu bylby problem
					msg[SECTION] = 1;
					msg[ID] = processID;
					MPI_Send(msg, MSG_SIZE, MPI_INT, i, MSG_RELEASE, MPI_COMM_WORLD );
					printf("Wątek %d wysłał RELEASE do wątku %d  \n",processID,i);
				}
			}
			//u siebie
			hospitalList.remove_if([](sectionRequest item){ return item.id == processID;});
		}else{
			//TO DO jak nie dostanie zgody na wejście
			cout << "Nie mam zgody na wejscie do sekcji krytycznej hospital. ID = " << processID << endl;

			cout << "W mojej kolejce (ID=)"<< processID<<"\n" << hospitalList << endl << endl;

		}

		int waitTime = random(10,20);
		cout << "Wątek " << processID << ", czekam " << waitTime << " sek. na kolejne wejście do sekcji\n";
		sleep(waitTime);

		//release
	}

		
	printf("Wątek %d zakończył pracę \n",processID);
	
	pthread_cancel(communicationThread);
	status=1;

	MPI_Finalize();
	return 0;
}
