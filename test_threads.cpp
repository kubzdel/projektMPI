#include <iostream>
#include <unistd.h>

#include <signal.h>
#include <mutex>
#include <condition_variable>

using namespace std;

pthread_t thread1_var;
pthread_t thread2_var;
condition_variable cv;
mutex m;
int value = -1;
void signalHandler(int dummy)
{
	cout << "Signal " << dummy << " pid: " << getpid() << endl;
	//int a = pthread_cancel(thread1_var);
    //int b = pthread_cancel(thread2_var);
	//cout << "Result " << a << " " << b << endl;
    exit(0);
}
int magic = 5;
void *thread1(void *)
{
    cout << "thread 1 \n";
    sleep(1);
    
    unique_lock<mutex> lk(m);
    cout << "Mutex in the critical section, thread1\n";
    cv.wait(lk, [] { return value == magic; });
    cout << "Thread 1 has been notified! \n ";
    lk.unlock();

    cout << "ending thread 1\n";
}

void *thread2(void *)
{
    sleep(2);
    m.lock();
    cout << "thread 2 in critical section" << endl;
    do{
        value = rand()%20;
        cout << value << endl;
        if(value ==  magic)
        {

            cout << "notyfing from thread 2\n";
            cv.notify_one();
        }else
        {
            cout << "nope, value = " << value << endl;
        }
        sleep(1);
    }while( value != magic );
    m.unlock();
    cout << "ending thread 2\n";
}

int main( int argc, char **argv )
{

	signal(SIGINT, signalHandler);
    cout << "Main thread " << endl;


    pthread_create(&thread1_var, NULL, thread1, NULL);
    pthread_create(&thread2_var, NULL, thread2, NULL);

    pthread_join(thread1_var, NULL);
    pthread_join(thread2_var, NULL);
    

    cout << "Zakończono oba wątki nara\n";
    return 0;
}