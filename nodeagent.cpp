#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string>

#include "common.h"

using namespace std;

class NodeAgent
{
	private:
		string serverName;
		string serverIpAddr;
		short int serverPort;
	public:
		NodeAgent(const char* hostName, const char* ipAddr, short port): 
				serverName(hostName), 
				serverIpAddr(ipAddr), 
				serverPort(port)
		{
		}
		void start()
		{
			int sockFd;
			struct sockaddr_in srvrAddr;
			char buff[MAXLENGTH] = {0};	
			
			sockFd = socket(AF_INET, SOCK_STREAM, 0);
			if(sockFd == -1) ERROR("can not create socket");

			bzero(&srvrAddr, sizeof(srvrAddr));
			srvrAddr.sin_family = AF_INET;
			//srvrAddr.sin_addr.s_addr = htonl(INADDR_ANY);
			inet_pton(AF_INET, serverIpAddr.c_str(), &srvrAddr.sin_addr);
			srvrAddr.sin_port = htons(serverPort);

			int ret = connect(sockFd, (struct sockaddr*)&srvrAddr, sizeof(srvrAddr));
			if(ret == -1) ERROR("connect faild");
			printf("connected to server\n");
			for(int i=0; i<10; ++i)
			{
				bzero(buff, MAXLENGTH);	
				printf("Sending heartbeat %d\n", i);
				sprintf(buff, "%s - %d", "well, how are you", i);
			    write(sockFd, buff, strlen(buff));
				//bzero(buff, MAXLENGTH);
				//read(sockFd, buff, MAXLENGTH);
				//printf("message from server: %s\n", buff);
				sleep(2);
			}	
			close(sockFd);
		}

};

int main(int argc, char** argv)
{
	NodeAgent agent("localhost", "127.0.0.1", 11111);
	agent.start();	
}
