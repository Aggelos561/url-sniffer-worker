all:
	g++ -o worker worker.cpp myFunctions.cpp
	g++ -o sniffer sniffer.cpp myFunctions.cpp
	@echo Compiled Successfully

#Add any directory if you want with -p
run:
	./sniffer

sniffer:
	g++ -o sniffer sniffer.cpp myFunctions.cpp

worker:
	g++ -o worker worker.cpp myFunctions.cpp

#Change or add tlds for bash script
finder:
	bash finder.sh com

fifo_clean:
	rm -f ../Named_Pipes/*

results_clean:
	rm -f ../results/*