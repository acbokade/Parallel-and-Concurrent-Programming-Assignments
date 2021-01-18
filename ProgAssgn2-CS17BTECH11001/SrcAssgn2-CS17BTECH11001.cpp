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
using namespace std;


/*
// Event record class
class EventRecord {
public:
    // stores time of event
    time_t time;
    // stores message to be printed 
    string message;
    // constructor
    EventRecord(string message, time_t time) {
        this->message = message;
        this->time = time;
    }
};


// custom comparator function for sorting event records by time
bool compare(const EventRecord& EventRecord_1, const EventRecord& EventRecord_2) {
    long long time_1 = (long long) (EventRecord_1.time);
    long long time_2 = (long long) (EventRecord_2.time);
    return time_1 < time_2;
}
*/

// Abstract class Lock
class Lock {
public:
    // pure virtual functions lock and unlock
    virtual void lock(int threadID) = 0;
    virtual void unlock(int threadID) = 0;
};


// Peterson lock for 2 threads class
class PetersonLock : public Lock {
    bool flag[2];
    int victim;
public:
    PetersonLock() {
        flag[0] = false;
        flag[1] = false;
    }

    void lock(int i) {
        int j = 1-i;
        // thread i is interested hence setting flag to true
        flag[i] = true;
        // set itself as victim allowing other thread to enter CS
        victim = i;
        while(flag[j] && victim == i) {};
    }

    void unlock(int i) {
        // setting flag to false, no longer interested, allowing other thread to enter
        flag[i] = false;
    }
};


// Peterson tree lock class
class PetersonTreeLock : public Lock {
    // binary peterson locks which will be used in tree
    PetersonLock* petersonLocks;
public:
    // empty constructor
    PetersonTreeLock() {
    }

    // parameterized constructor
    PetersonTreeLock(int n) {
        // n threads
        // n-1 binary Peterson locks will be used in the tree
        this->petersonLocks = new PetersonLock[n-1];
        for(int i=0;i<n-1;i++)
            this->petersonLocks[i] = PetersonLock();
    }

    // lock method where thread acquires every peterson lock from leaf to root
    void lock(int i) {
        // acquire every lock from leaf till root
        int lockID = i;

        // acquiring every lock till root 
        while(lockID) {
            // each lock treats one thread as 0 and other as 1, 
            // hence we take modulo 2 of lockID to assign one thread as 0 and other as 1
            petersonLocks[(lockID-1)/2].lock(lockID%2);
            // setting lock as parent of current lock
            lockID = (lockID-1)/2;
        }
    }

    // unlock method where binary peterson locks are released from leaf to root
    /*
    void unlock(int i) {
        // release every lock from leaf to root
        int lockID = i;
        while(lockID) {
            petersonLocks[(lockID-1)/2].unlock(lockID%2);
            lockID = (lockID-1)/2;
        }
    }
    */

    // unlock method where binary peterson locks are released from root to leaf
    void unlock(int thread_id) {
        // storing all locks from leaf to root 
        vector<int> lock_ids;
        lock_ids.push_back(thread_id);
        int lock_id = thread_id;

        // pushing every lock till root
        while(lock_id) {
            // pushing lock to vector
            lock_ids.push_back((lock_id-1)/2);
            // setting lock as parent of curent lock
            lock_id = (lock_id-1)/2;
        }

        // release every lock from root to leaf excluding last which is just threadID
        for(int i=lock_ids.size()-1;i>=1;i--) {
            petersonLocks[lock_ids[i]].unlock(lock_ids[i-1]%2);
        }
    }

    // freeing the allocated memory for peterson locks
    ~PetersonTreeLock() {
        delete [] petersonLocks;
    }    
};


// Filter lock class
class FilterLock : public Lock {
    // levels array which store level of each thread
    int* level; 
    // victim array which store victim at each level
    int* victim;
    // no of threads
    int no_of_threads;
public:
    // empty constructor
    FilterLock() {
    }

    // parameterized constructor
    FilterLock(int no_of_threads) {
        // allocating space for level and victim arrays
        this->level = new int[no_of_threads];
        this->victim = new int[no_of_threads];
        this->no_of_threads = no_of_threads;
        // initially every thread has level 0
        for(int i=0;i<no_of_threads;i++)
            level[i]=0;
    }

    // locking method
    void lock(int thread_id) {
        for(int i=1;i<this->no_of_threads;i++) {
            level[thread_id] = i;
            victim[i] = thread_id;
            // spin while conflicts exist
            while(true) {
                bool conflict = false;
                for(int k=0;k<no_of_threads;k++) {
                    if(k == thread_id)
                        continue;
                    // wait till another thread exists at higher or equal level and victim is itself
                    if(level[k] >= i && victim[i] == thread_id)
                        conflict = true;
                }
                // going to next level if there is no conflict
                if(!conflict)
                    break;
            }
        }
    }

    // unlocking method
    void unlock(int thread_id) {
        level[thread_id] = 0;
    }

    // destructor
    ~FilterLock() {
        // deallocating space of level and victim arrays
        delete [] level;
        delete [] victim;
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
// distributed
void testCS(int thread_id, int actual_thread_id, int no_of_threads, int no_of_entries, double lambda_1, double lambda_2, Lock* lock_obj) {
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
        output_file<<i+1<<"th CS Entry Request at "<< request_entry_time->tm_hour<<":"<<request_entry_time->tm_min<<":"<<request_entry_time->tm_sec<<" by thread "<<actual_thread_id<<" (mesg 1)\n"<<flush;
        //fprintf(output_file, "%dth CS Entry Request at %.2d:%.2d:%.2d by thread %d\n", i, request_entry_time->tm_hour, request_entry_time->tm_min, request_entry_time->tm_sec, actual_thread_id);
        
        // Actual Entry in Critical Section
        lock_obj->lock(thread_id);
        auto high_res_actual_entry_time = chrono::high_resolution_clock::now();
        time_t actual_entry_time_t = time(0);
        tm* actual_entry_time = localtime(&actual_entry_time_t);
        sleep(exponential_1(generator));
        output_file<<i+1<<"th CS Entry at "<< actual_entry_time->tm_hour<<":"<<actual_entry_time->tm_min<<":"<<actual_entry_time->tm_sec<<" by thread "<<actual_thread_id<<" (mesg 2)\n"<<flush;
        //fprintf(output_file, "%dth CS Entry at %.2d:%.2d:%.2d by thread %d\n", i, actual_entry_time->tm_hour, actual_entry_time->tm_min, actual_entry_time->tm_sec, actual_thread_id);

        // CS enter time in seconds
        cs_enter_time += chrono::duration_cast<chrono::microseconds>( high_res_actual_entry_time - high_res_request_entry_time ).count()/(double)(pow(10,6));

        // Request Exit
        auto high_res_request_exit_time = chrono::high_resolution_clock::now();
        time_t request_exit_time_t = time(0);
        tm* request_exit_time = localtime(&request_exit_time_t);
        output_file<<i+1<<"th CS Exit Request at "<< request_exit_time->tm_hour<<":"<<request_exit_time->tm_min<<":"<<request_exit_time->tm_sec<<" by thread "<<actual_thread_id<<" (mesg 3)\n"<<flush;
        //fprintf(output_file,  "%dth CS Exit Request at %.2d:%.2d:%.2d by thread %d\n", i, request_exit_time->tm_hour, request_exit_time->tm_min, request_exit_time->tm_sec, actual_thread_id);

        lock_obj->unlock(thread_id);

        // Actual Exit 
        auto high_res_actual_exit_time = chrono::high_resolution_clock::now();
        time_t actual_exit_time_t = time(0);
        tm* actual_exit_time = localtime(&actual_exit_time_t);
        output_file<<i+1<<"th CS Exit at "<< actual_exit_time->tm_hour<<":"<<request_entry_time->tm_min<<":"<<request_entry_time->tm_sec<<" by thread "<<actual_thread_id<<" (mesg 4)\n"<<flush;
        //fprintf(output_file,  "%dth CS Exit at %.2d:%.2d:%.2d by thread %d\n", i, actual_exit_time->tm_hour, actual_exit_time->tm_min, actual_exit_time->tm_sec, actual_thread_id);
        
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
    
    // threads for filter lock
    thread filter_lock_threads[no_of_threads]; 

    // threads for peterson tree lock
    thread peterson_tree_lock_threads[no_of_threads];

    // instantiating filter lock
    FilterLock filter_lock(no_of_threads);

    // instantiating peterson tree lock
    PetersonTreeLock peterson_tree_lock(no_of_threads);

    // filter lock
    cs_enter_time = 0;
    cs_exit_time = 0;
    output_file<<"Filter Lock Output:\n"<<flush;
    for(int i=0;i<no_of_threads;i++) 
        filter_lock_threads[i] = thread(testCS, i, i+1, no_of_threads, no_of_entries, lambda_1, lambda_2, &filter_lock);
    for(int i=0;i<no_of_threads;i++)
        filter_lock_threads[i].join();
    // average cs entry time
    double average_cs_enter_time = (double)cs_enter_time/(double)(no_of_threads*no_of_entries);
    // average cs exit time
    double average_cs_exit_time = (double)cs_exit_time/(double)(no_of_threads*no_of_entries);
    cout<<"Filter"<<endl;
    cout<<"Entry: "<<average_cs_enter_time<<endl;
    cout<<"Exit: "<<average_cs_exit_time<<endl;
    
    // peterson tree lock
    cs_enter_time = 0;
    cs_exit_time = 0;
    output_file<<"\nPTL Output:\n"<<flush;
    for(int i=0;i<no_of_threads;i++) 
       peterson_tree_lock_threads[i] = thread(testCS, no_of_threads-1+i, i+1, no_of_threads, no_of_entries, lambda_1, lambda_2, &peterson_tree_lock);
    for(int i=0;i<no_of_threads;i++)
        peterson_tree_lock_threads[i].join();
    // average cs entry time
    average_cs_enter_time = (double)cs_enter_time/(double)(no_of_threads*no_of_entries);
    // average cs exit time
    average_cs_exit_time = (double)cs_exit_time/(double)(no_of_threads*no_of_entries);
    cout<<"Peterson"<<endl;
    cout<<"Entry: "<<average_cs_enter_time<<endl;
    cout<<"Exit: "<<average_cs_exit_time<<endl;

    // cleanup i.e. closing all the files
    input_file.close();
    output_file.close();
    return 0;
}