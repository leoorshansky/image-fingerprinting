fingerprint: fingerprint.cpp
	g++ -o fingerprint fingerprint.cpp -IpHash -lstdc++fs -lX11 -lpthread -lboost_serialization -lboost_program_options -lpHash -g -O3