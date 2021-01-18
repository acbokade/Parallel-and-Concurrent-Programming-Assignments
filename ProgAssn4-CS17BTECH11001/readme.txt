Comparison of CLH and MCS Locks

1) Input to the program is a file named "inp-params.txt,".
   Input consists of the parameters n, k, λ1, λ2. where n is the number of threads, k is the number of requests made by each thread, λ1 and λ2 are lambda values for delay values t1, t2 which are exponentially distributed with average of λ1 and λ2 seconds.

2) Compile the CLH code by executing following command:
   g++ -std=c++11 -pthread CLH-CS17BTECH11001.cpp -o clh

3) Compile the MCS code by executing following command:
   g++ -std=c++11 -pthread MCS-CS17BTECH11001.cpp -o mcs

3) Run the CLH executable by :
   ./clh
   Run the MCS executable by :
   ./mcs

4) Output file 'output.txt' which contains the output logs for corresponding lock and CS average and exit times are printed on stdout.

