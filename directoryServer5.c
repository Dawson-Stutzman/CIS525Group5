#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include "inet.h"
#include "common.h"
#include <sys/queue.h>

struct serverEntry 
{
	int socket;
    int portNo;
	char servername[MAX];
    SLIST_ENTRY(serverEntry) serverEntries;             /* Singly linked List. */
};





//Initalize the head of the Singly linked list
SLIST_HEAD(serverHead, serverEntry);
int addServer(int sockNo, int portNo, char *servername, struct serverHead *head);
struct serverEntry *servernp, *servertempNode;
int main(int argc, char **argv)
{
	struct serverHead shead;  /* Singly linked List head. */
	SLIST_INIT(&shead);


	int				sockfd, newsockfd, maxfd;
	unsigned int	clilen;
	struct sockaddr_in cli_addr, dir_serv_addr;
	char				s[MAX], un[MAX];
	fd_set			readset, clone;
	int true = 1;


	/*############### BIND CHATSERVER ###############*/

	/* Create communication endpoint for the Directory Server */
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("dirServer: can't open stream socket");
		exit(1);
	}

	/* Add SO_REAUSEADDR option to prevent address in use errors (modified from: "Hands-On Network
	* Programming with C" Van Winkle, 2019. https://learning.oreilly.com/library/view/hands-on-network-programming/9781789349863/5130fe1b-5c8c-42c0-8656-4990bb7baf2e.xhtml */
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&true, sizeof(true)) < 0) {
		perror("dirServer: can't set stream socket address reuse option");
		exit(1);
	}

	//Setup the address
	memset((char *) &dir_serv_addr, 0, sizeof(dir_serv_addr));
	dir_serv_addr.sin_family = AF_INET;
	dir_serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	dir_serv_addr.sin_port		= htons(SERV_TCP_PORT);

	//Bind socket for Directory Server
	if (bind(sockfd, (struct sockaddr *) &dir_serv_addr, sizeof(dir_serv_addr)) < 0) {
		perror("dirServer: can't bind local address");
		exit(1);
	}


	listen(sockfd, 5);
	//initialize and set the readset to sockfd
	FD_ZERO(&readset);
	FD_SET(sockfd, &readset);
	maxfd = sockfd;

	clilen = sizeof(cli_addr);

	for (;;) 
	{
		clone = readset;
		/* There should be a select call in here somewhere */
		if (select(maxfd + 1, &clone, NULL, NULL, NULL) > 0)
		{
			//############## ADD NEW USER LOGIC #############

			//if select shows that there is an incoming message in its result (clone)

			if (FD_ISSET(sockfd, &clone))
			{
				//setup new Client or chatServer
				clilen = sizeof(cli_addr);
				newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
				if (newsockfd < 0) 
				{
					perror("directoryServer: accept error");
					exit(1);
				}
				else
				{
					//printf("New Connection\n");
					FD_SET(newsockfd, &readset);
					//Since maxfd represents the highest file descriptor, it needs to be updated anytime a fd is added
					if (newsockfd > maxfd) 
					{
						maxfd = newsockfd;
					}
				}			
				fprintf(stderr, "Accepted client or chatServer connection from %s:%d\n", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
			}



			//Loop through every active connection (ending at maxfd)
			for (int i = 0; i <= maxfd; i++)
			{

				//make sure that it was selected in the select statement
				if ((i != sockfd) && FD_ISSET(i, &clone))
				{
					/* Read the request from the client */
					if (read(i, s, MAX) <= 0) 
					{
						printf("i from initial for loop: %d\n", i);

						//######### REMOVE INACTIVE SERVERS LOGIC #########
						//NOTE: There was initially a userdisconnect var here as well to check if the disconnection was in users or chatservers, but I ended up not using the users SLIST and removed it.
						//Thus the flag variable and logic remains for the time being.
						int serverDisconnect = 0;
						memset(un, 0, MAX);
						if (SLIST_EMPTY(&shead) != true)
						{
							SLIST_FOREACH(servernp, &shead, serverEntries)
							{
								printf("servernp->socket: %d\n", servernp->socket);
								if (servernp->socket == i) 
								{
									snprintf(un, MAX, "%s", servernp->servername);
									un[MAX-1] = '\0';
									servertempNode = servernp;
									serverDisconnect = i;
								}
							}
						}
				

						if (serverDisconnect != 0)
						{

							SLIST_REMOVE(&shead, servertempNode, serverEntry, serverEntries);
							//Free the memory
							free(servertempNode);
							if (maxfd == i)
							{
								//update maxfd if appropriate. (avoids seg fault)
								maxfd -= 1;
							}
							//cose the socket
							close(i);

							FD_CLR(i, &readset);
							servertempNode = NULL;
							serverDisconnect = 0;
							printf("chatServer has Disconnected from directoryServer.\n");
						}
						
						
						//I found this usefull for debugging, as it will print out the ongoing connections after a disconnect.
						SLIST_FOREACH(servernp, &shead, serverEntries)
						{
							printf("%s is still connected after disconnect.\n", servernp->servername);
						}
						if (SLIST_FIRST(&shead) == NULL)
						{
							printf("empty chatServer list\n");
						}
						printf("\n");
					}

					//############## PARSE INPUT LOGIC ##############
					else 
					{
						/* based on the first character of the client's message */
						switch(s[0])  
						{
							//Server initialization request
							case 's':
							char *temp = "\0";
							char srvnm[MAX] = "\0";
							int port = 0; 
							removeFirstLetter(s);
							temp = strtok(s, ";");
							if (temp != NULL) 
							{
								sscanf(temp, "%d", &port);			// [FIX] CHECK THE RETURN VALUE OF SSCANF
							}
							snprintf(temp, MAX, "%s", strtok(NULL, ";"));
							if (temp != NULL)
							{
								snprintf(srvnm, MAX, "%s", temp);
							}
							if (addServer(i, port, srvnm, &shead) == 0 )
							{
								if (maxfd == i)
								{
									//update maxfd if appropriate. (avoids seg fault)
									maxfd -= 1;
								}
								//cose the socket by sending code x
								snprintf(s, MAX, "x");
								s[MAX - 1] = '\0';
								write(i, s, MAX);
								close(i);
								//clear it from the readset
								FD_CLR(i, &readset);
								break;
							}
							break;

							//Client directory request
							case 'c':
							
							servertempNode=SLIST_FIRST(&shead);
							if (servertempNode == NULL) {
								snprintf(s, MAX, "\n [!] No servers are currently online\n");
								write(i, s, MAX);
							}
							else 
							{
								//Reset the string s entirely
								memset(s, 0, MAX);
								write(i, "[PORT#] - SERVERNAME\n", MAX);
								SLIST_FOREACH(servernp, &shead, serverEntries) 
								{
									memset(s, 0, MAX);

									snprintf(s, MAX, "[%d] - %s", servernp->portNo, servernp->servername);
									write(i, s, MAX);
								}
								write(i, "w", MAX);
								if (maxfd == i)
								{
									//update maxfd if appropriate. (avoids seg fault)
									maxfd -= 1;
								}
							close(i);
							FD_CLR(i, &readset);
							}
							break;
							default: snprintf(s, MAX, "Invalid request\n");
						}
					}
				}
			}
		}
	}
	close(newsockfd);
}


void removeFirstLetter(char *string)
{
	//Initially I tried using sizeof(string), but it ran into some strange character duplication
	memcpy(string, string + 1, MAX);
	string[MAX - 1] = '\0';
}

// Adds a new server and returns varias values based on existing users in the SLIST.
int addServer(int sockNo, int portNo, char *servername, struct serverHead *h)
{
	if (isServerNameUnique(servername, h) == 1)
	{
		//Allocate and assign the new chatServer
		struct serverEntry *newNode = malloc(sizeof(struct serverEntry));
		snprintf(newNode->servername, MAX, "%s", servername);
		newNode->portNo = portNo;
		newNode->socket = sockNo;
		//add the chatServer and return 2 if they are the first chatServer (1 if they aren't.)
		if (SLIST_EMPTY(h)) 
		{
			SLIST_INSERT_HEAD(h, newNode, serverEntries);
			printf("FIRST SERVER HAS CONNECTED: %s\n\n", servername);
			//Case 2 means they are the first chatServer

			return 2;
		} 
		else 
		{
			SLIST_INSERT_HEAD(h, newNode, serverEntries);
			printf("SECOND OR > SERVER HAS CONNECTED: %s\n\n", servername);
			return 1;
		}
	}
	else
	{
	printf("dirServer: chatServer attempted to connect with already-existing name \"%s\"\n", servername);
	return 0;
	}
}

//Checks if the server name / Topic is unique.
int isServerNameUnique(char *serverName, struct serverHead *h)
{
	SLIST_FOREACH(servernp, h, serverEntries)
	{

		if (strncmp(serverName, servernp->servername, MAX) == 0)
		{
			return 0;
		}
	}
	return 1;
}
