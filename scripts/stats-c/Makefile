all: cw_stat

.PHONY: clean
clean:
	rm -f cw_stat

cw_stat: cw_stat.cpp
	g++ -o cw_stat cw_stat.cpp -lzmq
