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
#include <signal.h>
#include <condition_variable>

#include "helpers.cpp"
#include "structs.h"

#define P 5
#define J 20
#define L 10
#define MSG_SIZE 3
#define MSG_HELLO 100
#define MSG_REQUEST_RELEASE 200
#define MSG_RESPONSE 300
#define MSG_RELEASE 400
#define MSG_REQUEST 500

//przy konwersji na msg
#define SECTION 0
#define ID 1
#define TAG 2

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
bool run = true;

std::list<sectionRequest> hospitalList;  
std::list<sectionRequest> teleporterList;
mutex hospital_mutex;
mutex transport_mutex;
mutex localclock_mutex;
condition_variable hospital_entrance_condition;

pthread_t *communication_thread = NULL;

void signalHandler(int dummy)
{
	cout << "Odebrano sygnał zakończenia w procesie " << processID << ", z pid: " << getpid() << endl;
	if(communication_thread != NULL)
	{
		pthread_cancel(*communication_thread);
	}
	int state;
	MPI_Initialized( &state );
	if(state){
		cout << "Kończenie pracy MPI, proces nr " << processID << endl;
		MPI_Finalize();
	}else
	{
		cout << "Mpi nie był zainicjalizowany!\n";
	}
	exit(0);
}

mutex hospital_add_to_queue;
void add_to_hospital_queue(sectionRequest request)
{
	hospital_add_to_queue.lock();
	hospitalList.push_back(request);
	hospitalList.sort();
	// if( hospitalList.front().id == processID )
	// {
	// 	hospital_entrance_condition.notify_one();	//wake up main thread and let us enter hospital section
	// }
	hospital_add_to_queue.unlock();
}

void *communicationThread(void *)
{
	//mpi sie uruchamia wolniej niz tworzy sie watek wiec trzeba sprawdzac czy processID jest rozny od -1
	while(run)
	{
		int msg[MSG_SIZE];
		MPI_Status status;
		MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MSG_REQUEST_RELEASE, MPI_COMM_WORLD, &status);
		
		//mamy jeden Recv do request i release wiec i tak zawsze odbieramy tablice, chociaz przy release wystraczy jedna liczba
		sectionRequest request;
		request.clock = msg[SECTION];

		localclock_mutex.lock();
		localClock = max_int(request.clock, localClock);
		localClock++;
		localclock_mutex.unlock();
		
		request.section = msg[ID];
		request.id = status.MPI_SOURCE;
		request.tag = msg[TAG];
		int id = request.id;
		printf("Wątek %d otrzymał żądanie od wątku %d (typ %d) \n",processID,status.MPI_SOURCE, request.tag);

		if(request.tag == MSG_REQUEST)
		{
			if(request.section == 1)
			{
				add_to_hospital_queue(request);
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
		else if(request.tag == MSG_RELEASE)
		{
			cout << processID <<": Usuwam proces nr " << request.id << " z kolejki " << (request.section == 0 ? "teleportera" : "szpitala") << endl;
			//usun goscia z kolejki
			switch(request.section)
			{
				case HOSPITAL_STATUS:
					hospital_mutex.lock();
					hospitalList.remove_if([&request](sectionRequest item){ return item.id == request.id;});
					hospital_mutex.unlock();
					if( hospitalList.front().id == processID )
					{
						cout << "W procesie " << processID << " następuje notify one" << endl;
						hospital_entrance_condition.notify_one();	//wake up main thread and let us enter hospital section
					}
					
					break;
				case TELEPORTER_STATUS: 
					transport_mutex.lock();
					teleporterList.remove_if([&request](sectionRequest item){ return item.id == request.id;});
					transport_mutex.unlock();
					break;
				default: break;
			}
		}
		usleep(1000); //1ms
	}
}

pthread_t initParallelThread()
{
	communication_thread = new pthread_t;
	pthread_create(communication_thread, NULL, communicationThread, NULL);
	return *communication_thread;
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
	//handler na ctrl+c
	signal(SIGTERM, signalHandler);
	int provided;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);

	if (provided < MPI_THREAD_MULTIPLE)
	{
    	cout << "ERROR: MPI nie ma pełnego wsparcia dla wielowątkowości\n";
    	MPI_Abort(MPI_COMM_WORLD, 1);
		exit(0);
	}

	MPI_Comm_rank( MPI_COMM_WORLD, &processID );
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);

	status=0;
	
	//czekaj na inicjalizacje
	srand(random_int());
	int waitTime = random(5, 25);
	printf("Proces z id = %d, czekanie %.2f sek. na rozpoczęcie.\n", processID, waitTime / 10.f);
	usleep(waitTime*100 * 1000); //0.5s-2.5s
	
	pthread_t communicationThread = initParallelThread();
	
	while(run){
		sectionRequest hospitalPlace;
		localclock_mutex.lock();
		localClock++;
		hospitalPlace.clock = localClock;
		localclock_mutex.unlock();
		hospitalPlace.section = 1;
		hospitalPlace.id = processID;
		add_to_hospital_queue(hospitalPlace);
		int msg[] = {hospitalPlace.clock,hospitalPlace.section, MSG_REQUEST};
		for(int i=0;i<world_size;i++)
		{
			if(i!=processID)
			{
				MPI_Send( msg, MSG_SIZE, MPI_INT, i, MSG_REQUEST_RELEASE, MPI_COMM_WORLD );
				printf("Wątek %d wysłał żądanie do wątku %d  \n",processID,i);
			}
		}

		for(int i=0;i<world_size-1;i++)
		{
			MPI_Status status;
			MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MSG_RESPONSE, MPI_COMM_WORLD, &status);
			printf("Wątek %d otrzymał potwierdzenie nr %d, od wątku %d \n",processID,i+1,status.MPI_SOURCE);
		}
		
		cout << "process " << processID << " dostal wszystkie potwierdzenia, lista\n" << hospitalList << endl;
			//zeby nie pobrac danej przed dodaniem lub podczas sortowania
		
		//to jest dziwne ale nie moge zrobic tutaj hospital_mutex.lock() bo wait oczekuje na typ unique_lock
		//unique lock jest wrapperem dla hospital_mutex, konstruktor od razu wykonuje hospital_mutex.lock()
		unique_lock<mutex> lock(hospital_mutex); 

		//czekamy na notify od release, chyba, że za pierwszym razem jesteśmy na początku kolejki
		hospital_entrance_condition.wait(lock, []{ return hospitalList.front().id == processID; });

		//jesteśmy w sekcji krytycznej szpitala, czekamy losowy czas
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
				msg[TAG] = MSG_RELEASE;
				MPI_Send(msg, MSG_SIZE, MPI_INT, i, MSG_REQUEST_RELEASE, MPI_COMM_WORLD );
				printf("Wątek %d wysłał RELEASE do wątku %d  \n",processID,i);
			}
		}
		//u siebie
		hospital_add_to_queue.lock();
		hospitalList.remove_if([](sectionRequest item){ return item.id == processID;});
		hospital_add_to_queue.unlock();
		
		lock.unlock(); // == hospital_mutex.unlock()

	}

	printf("Wątek %d zakończył pracę \n",processID);
	
	pthread_cancel(communicationThread);
	MPI_Finalize();
	return 0;
}
