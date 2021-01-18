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

// Stamped Reg class
template <class T>
class Stamped_Reg {
public:
    T value;
    long stamp;
    long thread_id;
};

// atomic Stamped Reg class
template <class T>
class StampedReg{
public:
    // atomic Stamped_Reg member 
    atomic<Stamped_Reg<T>> stamped_reg;

    // constructor
    StampedReg() {
        stamped_reg.store({0, 0, -1});
    }

    // parameterized constructor
    StampedReg(T value, long stamp, long thread_id) {
        stamped_reg.store({value, stamp, thread_id});
    }

    // copy constructor
    StampedReg(const StampedReg<T> &other) {
        stamped_reg.store(other.stamped_reg.load());
    }

    // copy assignment operator 
    StampedReg &operator=(const StampedReg<T>& other) {
        stamped_reg.store(other.stamped_reg.load());
        return *this;
    }

    // defining equality operator
    // bool operator== (const StampedReg& other) {
    //     return (stamped_reg.load().stamp == other.stamped_reg.load().stamp)
    //     && (stamped_reg.load().thread_id == other.stamped_reg.load().thread_id);
    // }

    // defining != operator
    bool operator!= (const StampedReg& other) {
        return (stamped_reg.load().stamp != other.stamped_reg.load().stamp)
        || (stamped_reg.load().thread_id != other.stamped_reg.load().thread_id);
    } 

};

// MRMW wait free snapshot class
template <class T>
class MRMW_WFSnapshot {
    // REG array
    vector<StampedReg<T>> Reg;
    // HelpSnap array consists of snapshot for each thread
    vector<vector<T>> HelpSnap;
public:
    MRMW_WFSnapshot() {}

    MRMW_WFSnapshot(T init) {
        for(int i=0;i<capacity;i++)
            Reg.push_back(StampedReg<T>(init, 0, -1));
        HelpSnap.assign(n_threads, vector<T>(capacity));
    }
    
    // collection of Reg array
    StampedReg<T>* collect() {
        StampedReg<T>* copy = new StampedReg<T>[capacity];
        for(int j=0;j<capacity;j++) {
            copy[j] = Reg[j];
        }
        return copy;
    }
    
    // update at given location with given value by given thread_id
    void update(int thread_id, int location, T value, long stamp) {
        StampedReg<T> old_value = Reg[location];
        // old_value.stamped_reg.load().thread_id = thread_id;
        // old_value.stamped_reg.load().value = value;
        // old_value.stamped_reg.load().stamp = stamp;
        StampedReg<T> new_value(value, stamp, thread_id);
        Reg[location] = new_value;
        // taking snapshot
        T* snap_shot = this->snapshot();
        // storing snapshot at given thread_id
        for(int i=0;i<capacity;i++)
            HelpSnap[thread_id][i] = snap_shot[i];
        //delete[] snap_shot;
    }


    // snapshot of Reg array
    T* snapshot() {
        // boolean array representing if some thread can help at given location of Reg array
        bool can_help[n_threads] = {false};
        // initial collect
        StampedReg<T>* aa = collect();
        while(true) {
            StampedReg<T>* bb = collect();
            bool clean_double_collect = true;

            for(int i=0;i<capacity;i++) {
                if(aa[i] != bb[i]) {
                    clean_double_collect = false;
                    break;
                }
            }

            // returning bb in case of clean double collect
            if(clean_double_collect) {
                //cout<<"clean collect"<<endl;
                T* result = new T[capacity];
                for(int j=0;j<capacity;j++) 
                    result[j] = bb[j].stamped_reg.load().value;
                //delete[] bb;
                return result;
            }

            for(int i=0;i<capacity;i++) {
                if(bb[i] != aa[i]) {
                    StampedReg<T> mismatched_loc = bb[i];
                    long thread_id = mismatched_loc.stamped_reg.load().thread_id;
                    // checking if given thread can help at given location
                    if(can_help[thread_id]) {
                        //cout<<"helped"<<endl;
                        T* result = new T[capacity];
                        for(int j=0;j<capacity;j++) 
                            result[j] = HelpSnap[thread_id][i];
                        return result;
                    }
                    else
                        can_help[thread_id] = true;
                }
            }
            //delete[] aa;
            aa = bb;
        }
    }

    ~MRMW_WFSnapshot() {
        Reg.clear();
        for(int i=0;i<n_threads;i++)
            HelpSnap[i].clear();
    }
};

// declare mrmw wait free snapshot object of size capacity
MRMW_WFSnapshot<int>* MRMW_snap_object;

// map containing writer thread events 
// key is time and value is long entry of type string
map<double, string> write_events;

void writer(int thread_id) {
    //srand(time(NULL));
    map<double, string> local_write_events;
    // stamp number for each thread
    int stamp = 0;
    // exponential_distribution for delay
    exponential_distribution<double> exponential_1((double)1/(double)lambda_1);
    while(!terminate_writer_thread) {
        stringstream local_output;
        int value = rand();
        int location = rand() % capacity;
        MRMW_snap_object->update(thread_id, location, value, stamp);
        auto enter_time = chrono::high_resolution_clock::now(); 
        time_t entry_time_t = time(0);
        tm* entry_time = localtime(&entry_time_t);
        local_output<<"Thr"<<thread_id<<"'s write of "<<value<<" on location "<<location<<" at "<<entry_time->tm_hour<<":"<<entry_time->tm_min<<":"<<entry_time->tm_sec<<"\n";
        double time_taken = chrono::duration_cast<chrono::microseconds>(enter_time - start).count();
        // time in seconds
        time_taken /= 1000000;
        // storing write event onto local events map
        local_write_events[time_taken] = local_output.str();
        //record system time and value v in local log
        sleep(exponential_1(generator));
        // incrementing stamp for current thread
        stamp++;
    }

    // using lock for writing events into map, since multiple threads can write onto map
    // and to guarantee safety, used lock although it won't affect output result times
    output_file_lock.lock();
    for(auto event:local_write_events)
        write_events[event.first] = event.second;
    output_file_lock.unlock();
    //output_file<<local_output.rdbuf();
}

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
        int* collect = MRMW_snap_object->snapshot();
        
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

    // instantiating MRMW Snapshot object
    MRMW_snap_object = new MRMW_WFSnapshot<int>(0);

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

    // write all log entries onto file
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