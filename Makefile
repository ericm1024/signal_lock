test: test.cpp signal_lock.hpp
	clang++ -std=c++11 -lpthread -Wall -Wextra -pedantic test.cpp -o test
