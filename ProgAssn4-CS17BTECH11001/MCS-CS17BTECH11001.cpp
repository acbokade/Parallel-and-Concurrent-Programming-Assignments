#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <ctime>
#include <math.h>
#include <unistd.h>
#include <random>
#include <vector>
#include <algorithm>
#include <mutex>
#include <atomic>
using namespace std;

// QNode class
class QNode {
public:
    // locked begin true indicates thread has either 
    // acquired lock or waiting for lock
    // false indicates that thread has released it
    bool locked;
    // pointer of next node since in MCS lock
    // explicitly linked list is created 
    QNode* next;

    QNode() {
        locked = true;
        next = NULL;
    }
};

// declaring thread_local my_node for threads
static thread_local QNode* my_node = new QNode();

class MCSLock {
    // atomic tail node pointer
    atomic<QNode*> tail;
public:

    MCSLock() {
        // tail is initialized to NULL
        tail.store({NULL});
    }

    // my_node is thread local node 
    void lock() {
        QNode* pred = atomic_exchange(&tail, my_node);
        if(pred != NULL) {
            my_node->locked = true;
            pred->next = my_node;
            while(my_node->locked) {}
        }
    }

    // my_node is thread local node
    void unlock() {
        if(my_node->next == NULL) {
            if(tail.compare_exchange_strong(my_node, NULL)) return;
            while(my_node->next == NULL) {}
        }
        my_node->next->locked = false;
        my_node->next = NULL;
    }


    ~MCSLock() {
        // freeing tail pointer
        delete tail;
    }

};


// Critical section entry time taken by each thread
double cs_enter_time;
// Critical section exit time taken by each thread
double cs_exit_time;

// output file stream
ofstream output_file;

// random number generator
default_random_engine generator;

mutex increment_lock;

/***************************************************************
TO CHECK CORRECTNESS COMMENT OUT MESSAGE 1 AND MESSAGE 4
SINCE IF ALL MESSAGES ARE PRINTED THEN INTERLEAVING OF CHARACTER 
MAY HAPPEN SINCE COUT DOESN'T USE LOCKS.
FPRINTF CAN BE USED IF OUTPUT MESSAGES ARE IMPORTANT SINCE IT
INTERNALLY USES LOCKS.
FOR CORRECTNESS I.E. TO ENSURE MUTUAL EXCLUSION, MESSAGE 2 
AND 3 SHOULDN'T BE INTERLEAVED. HENCE MESSAGES 1 AND 4 CAN
BE COMMENTED.
****************************************************************/


// test function for critical section where thread enters the CS no_of_entries times
// and lambda_1 is average of delay for simulating CS task which is exponentially distributed
// and similarly lambda_2 is average of delay for exit section which is also exponentially
// distributed and my_nodes is local my_node for thread
void testCS(int thread_id, int no_of_entries, double lambda_1, double lambda_2, MCSLock* mcs_lock) {
    // exponential_distribution for Critical section sleep delay
    exponential_distribution<double> exponential_1((double)1/(double)lambda_1);
    // exponential_distribution for Exit section sleep delay
    exponential_distribution<double> exponential_2((double)1/(double)lambda_2);
    // Thread requesting to enter CS for no_of_entries times
    // comment msg 1 and msg 4 to check correctness 
    // interleaving of characters may happen if all messages from 1 to 4 are printed
    for(int i=0;i<no_of_entries;i++) {

        // Request Entry
        auto high_res_request_entry_time = chrono::high_resolution_clock::now();
        time_t request_entry_time_t = time(0);
        tm* request_entry_time = localtime(&request_entry_time_t);
        //output_file<<i+1<<"th CS Entry Request at "<< request_entry_time->tm_hour<<":"<<request_entry_time->tm_min<<":"<<request_entry_time->tm_sec<<" by thread "<<thread_id<<" (mesg 1)\n"<<flush;
        
        // Actual Entry in Critical Section
        mcs_lock->lock();
        auto high_res_actual_entry_time = chrono::high_resolution_clock::now();
        time_t actual_entry_time_t = time(0);
        tm* actual_entry_time = localtime(&actual_entry_time_t);
        sleep(exponential_1(generator));
        output_file<<i+1<<"th CS Entry at "<< actual_entry_time->tm_hour<<":"<<actual_entry_time->tm_min<<":"<<actual_entry_time->tm_sec<<" by thread "<<thread_id<<" (mesg 2)\n"<<flush;
    
        // CS enter time in seconds
        cs_enter_time += chrono::duration_cast<chrono::microseconds>( high_res_actual_entry_time - high_res_request_entry_time ).count()/(double)(pow(10,6));

        // Request Exit
        auto high_res_request_exit_time = chrono::high_resolution_clock::now();
        time_t request_exit_time_t = time(0);
        tm* request_exit_time = localtime(&request_exit_time_t);
        output_file<<i+1<<"th CS Exit Request at "<< request_exit_time->tm_hour<<":"<<request_exit_time->tm_min<<":"<<request_exit_time->tm_sec<<" by thread "<<thread_id<<" (mesg 3)\n"<<flush;

        mcs_lock->unlock();

        // Actual Exit 
        auto high_res_actual_exit_time = chrono::high_resolution_clock::now();
        time_t actual_exit_time_t = time(0);
        tm* actual_exit_time = localtime(&actual_exit_time_t);
        //output_file<<i+1<<"th CS Exit at "<< actual_exit_time->tm_hour<<":"<<request_entry_time->tm_min<<":"<<request_entry_time->tm_sec<<" by thread "<<thread_id<<" (mesg 4)\n"<<flush;
        
        // CS exit time in microseconds, since it is in order of microseconds
        // since cs_exit_time is shared variable, need to add a lock
        // no lock needed in case of cs_entry_time since it is updated inside lock
        increment_lock.lock();
        cs_exit_time += chrono::duration_cast<chrono::microseconds>( high_res_actual_exit_time - high_res_request_exit_time ).count();
        increment_lock.unlock();
        sleep(exponential_2(generator));
    }
}


int main() {
    // seed for default random engine generator
    generator.seed(4);

    // input file stream
    ifstream input_file;
    input_file.open("inp-params.txt");
    // output file stream
    output_file.open("output.txt");

    // parameters of input file
    int no_of_threads, no_of_entries;
    double lambda_1, lambda_2;

    input_file >> no_of_threads >> no_of_entries >> lambda_1 >> lambda_2;
    //cout<<no_of_threads<<" "<<no_of_entries<<" "<<lambda_1<<" "<<lambda_2<<endl;

    // threads for MCS lock
    thread MCS_threads[no_of_threads]; 

    // initializing MCS Lock
    MCSLock* mcs_lock = new MCSLock();

    // MCS lock
    cs_enter_time = 0;
    cs_exit_time = 0;
    output_file<<"MCS Lock Output:\n"<<flush;

    for(int i=0;i<no_of_threads;i++) {
        MCS_threads[i] = thread(testCS, i, no_of_entries, lambda_1, lambda_2, mcs_lock);
    }

    for(int i=0;i<no_of_threads;i++)
        MCS_threads[i].join();


    // average cs entry time
    double average_cs_enter_time = (double)cs_enter_time/(double)(no_of_threads*no_of_entries);
    // average cs exit time
    double average_cs_exit_time = (double)cs_exit_time/(double)(no_of_threads*no_of_entries);
    cout<<"MCS Lock"<<endl;
    cout<<"Average Entry time (in seconds): "<<average_cs_enter_time<<endl;
    cout<<"Average Exit time (in seconds): "<<average_cs_exit_time<<endl;

    // cleanup i.e. closing all the files
    input_file.close();
    output_file.close();
    return 0;
}