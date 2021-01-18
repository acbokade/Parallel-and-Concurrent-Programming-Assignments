#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <ctime>
#include <math.h>
#include <unistd.h>
#include <random>
#include <atomic>
#include <sstream>
#include <map>
#include <vector>
#include <algorithm>
using namespace std;

// random number generator
default_random_engine generator;

// output file stream
ofstream output_file;

// parameters of input file
int n_threads, capacity, n_snapshots;
double lambda_1, lambda_2;

// mutex lock for output file writing
mutex output_file_lock;

// to inform writer threads to terminate
atomic<bool> terminate_writer_thread(false);

// start time used to sort the write events
chrono::high_resolution_clock::time_point start;

// average waiting time for snapshot
double avg_completion_time;

// worst case time for snapshot
double worst_case_time;

// Stamped Snap class
template <class T>
class Stamped_Snap {
public:
    long stamp;
    T value;
    T* snap;
};

// used wrapper for Stamped_snap to make it trivially copyable
// atomic Stamped Snap class
template <class T>
class StampedSnap{
public:
    // atomic Stamped_Snap member 
    atomic<Stamped_Snap<T>> stamped_snap;

    // constructor
    StampedSnap() {
        stamped_snap.store({0, 0, NULL});
    }

    // parameterized constructor
    StampedSnap(long stamp, T value, T snap[]) {
        stamped_snap.store({stamp, value, snap});
    }

    // copy constructor
    StampedSnap(const StampedSnap<T> &other) {
        stamped_snap.store(other.stamped_snap.load());
    }

    // copy assignment operator 
    StampedSnap &operator=(const StampedSnap<T>& other) {
        stamped_snap.store(other.stamped_snap.load());
        return *this;
    }
};


// MRSW wait free snapshot class
template <class T>
class MRSW_WFSnapshot {
    vector<StampedSnap<T>> a_table;
public:
    MRSW_WFSnapshot() {}

    MRSW_WFSnapshot(T init) {
        for(int i=0;i<capacity;i++) {
            a_table.push_back(StampedSnap<T>(0, init, NULL));
        }
    }
    
    // collection of a_table array
    StampedSnap<T>* collect() {
        StampedSnap<T>* copy = new StampedSnap<T>[capacity];
        for(int j=0;j<capacity;j++) {
            copy[j] = this->a_table[j];
        }
        return copy;
    }
    
    // snapshot 
    T* scan() {
        // boolean array to indicate if thread has updated twice 
        bool moved[capacity] = {false};
        // old snapshot
        StampedSnap<T>* old_copy = collect();
        bool clean_double_collect = true;

        while(true) {
            clean_double_collect = true;
            // new snapshot
            StampedSnap<T>* new_copy = collect();
            for(int j=0;j<capacity;j++) {
                if(old_copy[j].stamped_snap.load().stamp != new_copy[j].stamped_snap.load().stamp) {
                    // if atleast one of the register's stamp is not equal, then its not clean double collect
                    clean_double_collect = false;
                    // case of double update by some thread
                    if(moved[j]) {
                        //cout<<"double move"<<endl;
                        //delete[] old_copy;
                        return new_copy[j].stamped_snap.load().snap;
                    } else {
                        moved[j] = true;
                        //delete[] old_copy;
                        old_copy = new_copy;
                    }
                }
            }
            // returning in case of clean double collect
            if(clean_double_collect) {
                //cout<<"clean collect"<<endl;
                //delete[] old_copy;
                T* result = new T[capacity];
                for(int j=0;j<capacity;j++) 
                    result[j] = new_copy[j].stamped_snap.load().value;
                //delete[] new_copy;
                return result;
            }
        }
    }

    // update at given thread_id location with given value
    void update(int thread_id, T value) {
        // take snapshot
        T* snap = this->scan();
        StampedSnap<T> old_value = this->a_table[thread_id];
        // obtaining stamp of old value
        long stamp = old_value.stamped_snap.load().stamp;
        T* old_snap = old_value.stamped_snap.load().snap;
        //delete[] old_snap;
        // incrementing stamp
        StampedSnap<T> new_value(stamp+1, value, snap);
        this->a_table[thread_id] = new_value;
    }
    
    ~MRSW_WFSnapshot() {
        a_table.clear();
    }

};

// declaring MRSW snapshot object
MRSW_WFSnapshot<int>* MRSW_snap_object;

// map containing writer thread events 
// key is time and value is long entry of type string
map<double, string> write_events;


// writer thread
void writer(int thread_id) {
    //srand(time(NULL));
    map<double, string> local_write_events;
    // exponential_distribution for delay
    exponential_distribution<double> exponential_1((double)1/(double)lambda_1);
    while(!terminate_writer_thread) {
        // thread local string stream
        stringstream local_output;
        int value = rand();
        //int location = rand() % capacity;
        MRSW_snap_object->update(thread_id, value);
        auto enter_time = chrono::high_resolution_clock::now(); 
        time_t entry_time_t = time(0);
        tm* entry_time = localtime(&entry_time_t);
        local_output<<"Thr"<<thread_id<<"'s write of "<<value<<" "<< "at "<<entry_time->tm_hour<<":"<<entry_time->tm_min<<":"<<entry_time->tm_sec<<"\n";
        double time_taken = chrono::duration_cast<chrono::microseconds>(enter_time - start).count();
        // time in seconds
        time_taken /= 1000000;
        // storing write event onto local events map
        local_write_events[time_taken] = local_output.str();
        //record system time and value v in local log
        sleep(exponential_1(generator));
    }

    // using lock for writing events into map, since multiple threads can write onto map
    // and to guarantee safety, used lock although it won't affect output result times
    output_file_lock.lock();
    for(auto event:local_write_events)
        write_events[event.first] = event.second;
    output_file_lock.unlock();
    // output_file<<local_output.rdbuf();
}


// snapshot thread
void snapshot() {
    // count of snapshots
    int count = 0;
    // local map for log entries
    map<double, string> local_write_events;
    // exponential_distribution for delay
    exponential_distribution<double> exponential_2((double)1/(double)lambda_2);
    while(count < n_snapshots) {
        // thread local string stream
        stringstream local_output;
        // begin collect time
        auto high_res_begin_collect_time = chrono::high_resolution_clock::now();
        time_t begin_collect_time_t = time(0);
        tm* begin_collect_time = localtime(&begin_collect_time_t);
        
        // call snapshot 
        int* collect = MRSW_snap_object->scan();
        
        // end collect time
        auto high_res_end_collect_time = chrono::high_resolution_clock::now();
        time_t end_collect_time_t = time(0);
        tm* end_collect_time = localtime(&end_collect_time_t);

        // time taken to collect snapshot
        double time_taken = chrono::duration_cast<chrono::microseconds>(high_res_end_collect_time - high_res_begin_collect_time).count();
        avg_completion_time += time_taken;
        worst_case_time = max(worst_case_time, time_taken);

        local_output<<"Snapshot Thr's snapshot: ";
        for(int i=0;i<capacity;i++)
            local_output<<"l"<<i<<"-"<<collect[i]<<" ";
        local_output<<" which finished at "<<end_collect_time->tm_hour<<":"<<end_collect_time->tm_min<<":"<<end_collect_time->tm_sec<<"\n";
        
        double time_diff = chrono::duration_cast<chrono::microseconds>(high_res_end_collect_time - start).count();
        // time in seconds
        time_diff /= 1000000;
        // storing write event onto local events map
        local_write_events[time_diff] = local_output.str();

        //delete[] collect;

        sleep(exponential_2(generator));
        count++;
    }
    // using lock for writing events into map, since multiple threads can write onto map
    // and to guarantee safety, used lock although it won't affect output result times
    output_file_lock.lock();
    for(auto event:local_write_events)
        write_events[event.first] = event.second;
    output_file_lock.unlock();
}


int main() {
    // seed for default random engine generator
    generator.seed(4);
    
    // input file stream
    ifstream input_file;
    input_file.open("inp-params.txt");
    
    // output file stream
    output_file.open("output.txt");

    // lambda_1 is average delay for writer thread and lambda_2 is average delay for snapshot thread
    input_file >> n_threads >> capacity >> lambda_1 >> lambda_2 >> n_snapshots;
    
    //capacity = n_threads;

    // instantiating MRSW Snapshot object
    MRSW_snap_object = new MRSW_WFSnapshot<int>(0);

    // writer threads
    thread writer_threads[n_threads];
    // one snapshot thread
    thread snapshot_thread;
    
    start = chrono::high_resolution_clock::now();
    avg_completion_time = 0.0;
    worst_case_time = 0;

    // creating writer threads
    for(int i=0;i<n_threads;i++)
        writer_threads[i] = thread(writer, i);

    // creating snapshot thread
    snapshot_thread = thread(snapshot);

    // wait until snapshot thread terminates;
    snapshot_thread.join();

    // inform writer threads that they have to terminate
    terminate_writer_thread = true; 
    
    // wait until all writer threads terminate
    for(int i=0;i<n_threads;i++)
        writer_threads[i].join();

    // write all writer log entries onto file
    // Log entries are sorted by their time of entry
    for(auto event:write_events) {
        output_file<<event.second;
    }

    write_events.clear();

    avg_completion_time /= n_snapshots;
    cout<<"Average waiting time for taking snapshot is "<<avg_completion_time<<endl;
    cout<<"Worst case time to take snapshot is "<<worst_case_time<<endl;

    // cleanup i.e. closing all the files
    input_file.close();
    output_file.close();

    return 0;
}