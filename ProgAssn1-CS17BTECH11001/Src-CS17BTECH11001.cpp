#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <ctime>
#include <math.h>
using namespace std;

// output files stream
ofstream primes_DAM_output_file;
ofstream primes_SAM1_output_file;
ofstream primes_SAM2_output_file;

// mutex lock for counter
mutex counter_lock;
// mutex lock for DAM output file
mutex DAM_file_lock;
// mutex lock for SAM1 output file
mutex SAM1_file_lock;
// mutex lock for SAM2 output file
mutex SAM2_file_lock;

// counter class
class Counter {
    long value;
public:
    Counter(int val) {
        value = val;
    }
    long getAndIncrement() {
        // critical section 
        counter_lock.lock();
        int ret_val = value++;
        counter_lock.unlock();
        return ret_val;
    }
};

// primality test of number n i.e. check if n is prime or not
bool checkPrimality(int n) {
    // if any number i between 2 to square root of n divides
    // n, then its composite
    for(int i=2;i*i<=n;i++) {
        if(n%i == 0)
            return false;
    }
    return true;
}

// dynamic allocation method where each thread gets a prime number to test dynamically
void DAM(Counter* counter, int n, int threadId) {
    while(true) {
        // calling counter getAndIncrement to obtain number to test
        int counter_val = counter->getAndIncrement();
        // returning if number is greater than n
        if(counter_val > n)
            return;
        if(checkPrimality(counter_val)) {
            // critical section
            // append number to output file
            DAM_file_lock.lock();
            primes_DAM_output_file<<counter_val<<" ";
            DAM_file_lock.unlock();
        }
    }
}

// static allocation method1 where each thread gets prime number to test in gap of threads count
void SAM1(int n, int noOfThreads, int threadId) {
    int count = 0;
    while(true) {
        // next number to test is in gap of noOfThreads
        int next_number = count*noOfThreads + threadId;
        // breaking if number is greater than n
        if(next_number > n)
            break;
        if(checkPrimality(next_number)) {
            // critical section
            // append number to output file
            SAM1_file_lock.lock();
            primes_SAM1_output_file<<next_number<<" ";
            SAM1_file_lock.unlock();
        }
        count++; 
    }
}

// static allocation method2 where threads are given only odd numbers to test in gap of threads count
void SAM2(int n, int noOfThreads, int threadId) {
    int count = 0;
    while(true) {
        // next number to test is in gap of 2*noOfThreads since even numbers are skipped
        int next_number = 2*count*noOfThreads + (2*threadId-1);
        // breaking if number is greater than n
        if(next_number > n)
            break;
        if(checkPrimality(next_number)) {
            // critical section
            // append number to output file
            SAM2_file_lock.lock();
            primes_SAM2_output_file<<next_number<<" ";
            SAM2_file_lock.unlock();
        }
        count++; 
    }
}

int main() {
    // n is number upto which prime numbers are to be tested
    int n, noOfThreads;
    // input file stream
    ifstream input_file;
    // time output file stream
    ofstream time_output_file;


    input_file.open("inp-params.txt");
    primes_DAM_output_file.open("Primes-DAM.txt");
    primes_SAM1_output_file.open("Primes-SAM1.txt");
    primes_SAM2_output_file.open("Primes-SAM2.txt");
    time_output_file.open("Times.txt");

    // reading n and threads count from input file
    input_file>>n>>noOfThreads;
    
    // instantiating counter with initial value 1
    Counter counter(1);

    // declaring threads with count given in input file 
    thread threads[noOfThreads];
    
    // measuring start time before calling DAM
    auto start_time = std::chrono::high_resolution_clock::now();
    // creating all threads
    for(int i=1;i<=noOfThreads;i++)
        threads[i-1] = thread(DAM, &counter, n, i);
    // joining all the threads
    for(int i=1;i<=noOfThreads;i++)
        threads[i-1].join();
    // measuring end time after DAM call
    auto end_time = std::chrono::high_resolution_clock::now();
    // calculating difference between end and star time
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>( end_time - start_time ).count()/(double)(pow(10,6));
    time_output_file<<duration<<" ";


    // measuring start time before calling SAM1
    start_time = std::chrono::high_resolution_clock::now();
    // creating all threads
    for(int i=1;i<=noOfThreads;i++)
        threads[i-1] = thread(SAM1, n, noOfThreads, i);
    // joining all the threads
    for(int i=1;i<=noOfThreads;i++)
        threads[i-1].join();
    // measuring end time after SAM1 call
    end_time = std::chrono::high_resolution_clock::now();
    // calculating difference between end and star time
    duration = std::chrono::duration_cast<std::chrono::microseconds>( end_time - start_time ).count()/(double)(pow(10,6));
    time_output_file<<duration<<" ";


    // measuring start time before calling SAM2
    start_time = std::chrono::high_resolution_clock::now();
    // creating all threads
    for(int i=1;i<=noOfThreads;i++)
        threads[i-1] = thread(SAM2, n, noOfThreads, i);
    // joining all the threads
    for(int i=1;i<=noOfThreads;i++)
        threads[i-1].join();
    // measuring end time after SAM2 call
    end_time = std::chrono::high_resolution_clock::now();
    // calculating difference between end and star time
    duration = std::chrono::duration_cast<std::chrono::microseconds>( end_time - start_time ).count()/(double)(pow(10,6));
    time_output_file<<duration<<endl;


    // cleanup, closing all file streams
    primes_DAM_output_file.close();
    primes_SAM1_output_file.close();
    time_output_file.close();
    input_file.close();
    return 0;
}