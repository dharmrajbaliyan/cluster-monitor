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
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>

#include "common.h"

#include<vector>
#include<algorithm>
#include <map>

using namespace std;

class HealthCHKServer 
{
	private:
		short int serverPort;	
		vector<int> watchFd;
		pthread_t watchertid;
		static int watchFdLock;
		static volatile bool stopWatcher;
		static int triggerFd;
		int pipefd[2];
	public:		
		HealthCHKServer(short int port):serverPort(port)
		{
			//triggerFd = open("/tmp/wtchr", O_RDWR | O_CREAT | O_TRUNC);	
			//if(triggerFd == -1) ERROR("watcher file open error");
			int ret = pipe(pipefd);
			if (ret == -1) ERROR("failed to create pipe");
			addFd(pipefd[0], false);
		}	  
		void start() 
		{
			//first start the watcher thread
			pthread_create(&watchertid, NULL, &HealthCHKServer::watcher, &watchFd);	
			int listenFd, connFd;
			struct sockaddr_in srvrAddr, clntAddr;
			char buff[MAXLENGTH] = {0};	
			
			listenFd = socket(AF_INET, SOCK_STREAM, 0);
			if(listenFd == -1) ERROR("can not create socket");

			bzero(&srvrAddr, sizeof(srvrAddr));
			srvrAddr.sin_family = AF_INET;
			srvrAddr.sin_addr.s_addr = htonl(INADDR_ANY);
			srvrAddr.sin_port = htons(serverPort);
			int srvrAddrLen = sizeof(srvrAddr);
			int ret = ::bind(listenFd, (struct sockaddr*)&srvrAddr, srvrAddrLen);
			if(ret == -1) ERROR("bind failed..");
			
			ret = listen(listenFd, LISTENQUEUE);
			if(ret == -1) ERROR("listen failed");

			while(true)
			{
				socklen_t length = sizeof(clntAddr);
				connFd = accept(listenFd, (struct sockaddr *)&clntAddr, &length);
				if(connFd == -1 && errno == EINTR)
				{
					perror("Signal interrupt");
					continue;	
				}
				else if(connFd == -1)
				{
					ERROR("accept error");	
				}
				printf("connection from %s, port %d\n", inet_ntop(AF_INET, &clntAddr.sin_addr, buff, sizeof(buff)), ntohs(clntAddr.sin_port));
				//read(connFd, buff, MAXLENGTH);
				//printf("message: %s\n", buff);
				//bzero(buff, MAXLENGTH);
				//sprintf(buff, "%s", "I am fine, evolving to event loop");
				//write(connFd, buff, strlen(buff));
				//Add to watcher
				addFd(connFd);
				//close(connFd);
			}
		}

		void addFd(int fd, bool triggerWatcher=true)
		{
			while(false == __sync_bool_compare_and_swap (&watchFdLock, 0, 1)) 
			{   
				sched_yield();
			}
			watchFd.push_back(fd);
			bool ret =  __sync_bool_compare_and_swap (&watchFdLock, 1, 0);
			assert(ret);					

			if(triggerWatcher)
			{	
				if(-1 == write(pipefd[1], "1", 1)) ERROR("failed to write triiger file");
			}
		}
		static void* detector(void* in)
		{
			//keep two vector
			//vector<bool> tells which fd watcher is using
			//vector<int> tells # of times worker reported health
			//define DOUBT, VERIFY, CONFIRM level
			//CONFIRM means we have lost the worker, start services on other nodes which were on this worker
			//detector sleeps 3*x time, ideally worker should report [2,3] health
			//if we get from a worker only zero, we go to DOUBT
			//if next time also we get zero from this worker, we go to VERIFY state
			//if next time also we get zero from this worker, we got to CONFIRM state and take action as described above
			map<int, string>& worker2FdMap = *(static_cast<map<int, string>*>(in));

		}

		static void* watcher(void* in)
		{
			vector<int>& watchFd = *(static_cast<vector<int>*>(in));	
			char buff[MAXLENGTH] = {0};
			fd_set rset;	
			int max1 = 0;
			while(!stopWatcher)
			{
				FD_ZERO(&rset);	
				while(false == __sync_bool_compare_and_swap (&watchFdLock, 0, 1)) 
				{   
					sched_yield();
				}
				//printf("Watching fd's : %u\n", watchFd.size());
				for(size_t i=0; i<watchFd.size(); ++i)
				{
					FD_SET(watchFd[i], &rset);	
				}	
				bool ret =  __sync_bool_compare_and_swap (&watchFdLock, 1, 0);
				assert(ret);					
				max1 = *(max_element(watchFd.cbegin(), watchFd.cend())) + 1;
				//select blocks untill one of the fd become readable.
				printf("Going to sleep in select!\n");
				int res = select(max1, &rset, NULL, NULL, NULL);
				if(res == -1) ERROR("select failed");

				//take lock, since now we are going to iterate the watchFds
				while(false == __sync_bool_compare_and_swap (&watchFdLock, 0, 1)) 
				{   
					sched_yield();
				}
				for(size_t i=0; i<watchFd.size(); ++i)
				{
					if(FD_ISSET(watchFd[i], &rset) && i == 0)
					{
						//Got one or more fd to watch, let us add them!	
						//we may have got heart beat from other mean while
						//so we'll continue loop
						printf("Got a new fd to monitor\n");
						bzero(buff, MAXLENGTH);	
						read(watchFd[i], buff, MAXLENGTH);	
						printf("message from %d: %s\n", watchFd[i], buff);
						break;
						//continue;
					}
					if(FD_ISSET(watchFd[i], &rset))
					{
						bzero(buff, MAXLENGTH);	
						int nbytes = read(watchFd[i], buff, MAXLENGTH);	
						if(nbytes == 0)
						{
							//Connection closed by remote end
							//remove from watcher list
							//fprintf(stderr, "Connection closed by remote end, fd : %d\n", watchFd[i]);
							printf("Connection closed by remote end, fd : %d\n", watchFd[i]);
							watchFd.erase(watchFd.begin() + i);
							close(watchFd[i]);
						}
						else
						{
							printf("message from %d: %s\n", watchFd[i], buff);
						}
						continue;
					}	
				}	
				//release lock
				ret =  __sync_bool_compare_and_swap (&watchFdLock, 1, 0);
				assert(ret);					
			}		
		}		

};

int HealthCHKServer::watchFdLock = 0;
int HealthCHKServer::triggerFd = 0;
volatile bool HealthCHKServer::stopWatcher = false;

int main(int argc, char** argv)
{
	//disable printf buffering	
	setbuf(stdout, NULL);

	HealthCHKServer hsrvr(11111);
	hsrvr.start();
		
}
