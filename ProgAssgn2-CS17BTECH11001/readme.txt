Implementing Filter and Peterson based Tree Lock Algorithms

1) Input to the program is a file named "inp-params.txt,".
   Input consists of the parameters n, k, λ1, λ2. where n is the number of threads, k is the number of requests made by each thread, λ1 and λ2 are lambda values for delay values t1, t2 which are exponentially distributed with average of λ1 and λ2 seconds.

2) Compile the CME code by executing following command:
   g++ -std=c++11 -pthread SrcAssgn2-CS17BTECH11001.cpp -o out

3) Run the CME executable by :
   ./out

4) Output file 'output.txt' which contains the output for Filter lock followed by Peterson Tree Lock and CS average and exit times are printed on stdout.

