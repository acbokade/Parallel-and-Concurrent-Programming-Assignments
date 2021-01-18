Wait-Free Atomic Snapshot Implementations Comparing the Solutions of MRSW and MRMW

1) Input to the program is a file named "inp-params.txt".
   Input consists of the parameters n, M, λw, λr, k where n is the number of writer threads, M is the size of snapshot register array, λw (writer thread) and λr (snapshot thread) are lambda values for delay values t1, t2 which are exponentially distributed with average of λw and λr seconds.

2) To Compile the MRSW code by executing following command:
   g++ -std=c++11 -pthread mrsw-CS17BTECH11001.cpp -latomic -o mrsw

3) To Compile the MRMW code by executing following command:
   g++ -std=c++11 -pthread mrmw-CS17BTECH11001.cpp -latomic -o mrmw

4) Run the MRSW executable by :
   ./mrsw
   
5) Run the MRMW executable by :
   ./mrmw

6) Output file 'output.txt' which contains logs written by snapshot thread and writer thread sorted by the times they have written those i.e. chronologically.

