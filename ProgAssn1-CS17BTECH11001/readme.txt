Comparing Different Parallel Implementations for Identifying Prime Numbers

1) Input to the program is a file named "inp-params.txt,".
   Input consists of the parameters n, m where 10**n = N. Here N is the number below which you to find the prime number of primes and m is the number of threads that needs to be created.

2) Compile the CME code by executing following command:
   g++ -std=c++11 -pthread Src-CS17BTECH11001.cpp -o out

3) Run the CME executable by :
   ./out

4) Output files 'Primes-DAM.txt', 'Primes-SAM1.txt', 'Primes-SAM2.txt', 'Times.txt' are generated. The prime numbers in the file are separated by space as follows: <PrimeNumber1 PrimeNumber2 ...>. Times file consists of <Time1 Time2 Time3> which are time taken by DAM & SAM1 & SAM2 algorithms (in seconds) respectively.

