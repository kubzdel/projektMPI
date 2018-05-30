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

#define P 2
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
#define CLOCK 1
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


//do wyswietlania elementów kolejki
std::ostream& operator<<(std::ostream& ostr, const std::list<sectionRequest>& list)
{
    for (auto &i : list) {
        ostr <<  "\t mojeID=" << processID << "\tproc = " << i.id << " zegar=" << i.clock << endl;
    }
    return ostr;
}

string invocation(){
	return "[Proces " + std::to_string(processID) + " ] ";  
}

mutex hospital_add_to_queue;
void add_to_hospital_queue(sectionRequest request)
{
	hospital_mutex.lock();
	hospitalList.push_back(request);
	hospitalList.sort();
	hospital_mutex.unlock();
}

bool canEnterCriticalSection(std::list<sectionRequest> &sectionList, int maxProcessesInside)
{
	//cout << invocation() << "W sekcji krytycznej przy enter critical section \n" << sectionList << endl;
	int elementCount = 1;
	for(auto &item : sectionList)
	{
		if(elementCount > maxProcessesInside)
		{
			return false;
		}
		if(item.id == processID)
		{
			return true;
		}
		elementCount++;
	}
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
		request.clock = msg[CLOCK];

		localclock_mutex.lock();
		localClock = max_int(request.clock, localClock);
		localClock++;
		localclock_mutex.unlock();
		
		request.section = msg[SECTION];
		request.id = status.MPI_SOURCE;
		request.tag = msg[TAG];
		cout << invocation() << "otrzymał żądanie od procesu " << status.MPI_SOURCE << " (typ" << request.tag << ")" << endl; 
		
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
			cout << invocation << " odesłał potwierdzenie do procesu " << status.MPI_SOURCE << endl;
		}
		else if(request.tag == MSG_RELEASE)
		{
			cout << invocation() <<" usuwam proces nr " << request.id << " z kolejki " << (request.section == HOSPITAL_STATUS ? "szpitala." : "teleportera.") << endl;
			//usun goscia z kolejki
			switch(request.section)
			{
				case HOSPITAL_STATUS:
					hospital_mutex.lock();
					hospitalList.remove_if([&request](sectionRequest item){ return item.id == request.id;});
					if( canEnterCriticalSection(hospitalList, P) )
					{
						cout << invocation() << " mam zezwolenie na wejście do sekcji krytycznej szpitala" << endl;
						hospital_entrance_condition.notify_one();	//wake up main thread and let us enter hospital section
					}
					else
					{
						cout << invocation() << "nie mogę wejść do sekcji krytycznej" << endl; 
					}
					hospital_mutex.unlock();
					
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
	printf("%s czekam  %.2f sek. na rozpoczęcie.\n", invocation().c_str(), waitTime / 10.f);
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
		int msg[MSG_SIZE];
		msg[SECTION]  = hospitalPlace.section;
		msg[CLOCK] = hospitalPlace.clock;
		msg[TAG] = MSG_REQUEST;
		for(int i=0;i<world_size;i++)
		{
			if(i!=processID)
			{
				MPI_Send( msg, MSG_SIZE, MPI_INT, i, MSG_REQUEST_RELEASE, MPI_COMM_WORLD );
				cout << invocation() << " wysyłam żądanie do procesu " << i << endl;
			}
		}

		for(int i=0;i<world_size-1;i++)
		{
			MPI_Status status;
			MPI_Recv(msg, MSG_SIZE, MPI_INT, MPI_ANY_SOURCE, MSG_RESPONSE, MPI_COMM_WORLD, &status);
			cout << invocation() << "otrzymałem potwierdzenie nr " << i+1 << " od procesu " << status.MPI_SOURCE << endl;
		}
		
		//cout << invocation() << " dostałem wszystkie potwierdzenia, lista\n" << hospitalList << endl;
			//zeby nie pobrac danej przed dodaniem lub podczas sortowania
		
		//to jest dziwne ale nie moge zrobic tutaj hospital_mutex.lock() bo wait oczekuje na typ unique_lock
		//unique lock jest wrapperem dla hospital_mutex, konstruktor od razu wykonuje hospital_mutex.lock()
		unique_lock<mutex> lock(hospital_mutex); 

		//czekamy na notify od release, chyba, że za pierwszym razem jesteśmy na początku kolejki
		hospital_entrance_condition.wait(lock, []{return canEnterCriticalSection(hospitalList, P); });

		//jesteśmy w sekcji krytycznej szpitala, czekamy losowy czas
		int waitTime = random(10,20);
		cout << "--------------------------->" << invocation() << " czekam " << waitTime << " sek. w SEKCJI KRYTYCZNEJ HOSPITAL\n";
		sleep(waitTime);
		cout << invocation() << " wyszedłem z sekcji krytycznej\n\n"; 
		//usun swoje żądanie z sekcji krytycznej
		//u reszty procesów
		for(int i=0;i<world_size;i++)
		{
			if(i!=processID)
			{
				//tu troche bezsensu wysylamy az dwa inty ale pozniej w odbieraniu bylby problem
				msg[SECTION] = HOSPITAL_STATUS;
				msg[TAG] = MSG_RELEASE;
				MPI_Send(msg, MSG_SIZE, MPI_INT, i, MSG_REQUEST_RELEASE, MPI_COMM_WORLD );
				//printf("Wątek %d wysłał RELEASE do wątku %d  \n",processID,i);
			}
		}
		//u siebie
		hospitalList.remove_if([](sectionRequest item){ return item.id == processID;});
		
		lock.unlock(); // == hospital_mutex.unlock()

	}

	cout << invocation() << " zakończyłem pracę\n";
	
	pthread_cancel(communicationThread);
	MPI_Finalize();
	return 0;
}
