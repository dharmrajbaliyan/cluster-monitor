
SRC_SERVER=healthchkserver.cpp
SRC_CLIENT=nodeagent.cpp
INC= -I.
CC=g++ -std=c++0x
all:
	$(CC)  -o server $(INC) $(SRC_SERVER) -levent -lpthread
	$(CC)  -o agent  $(INC) $(SRC_CLIENT) -levent
