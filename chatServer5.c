#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include "inet.h"
#include "common.h"
#include <sys/queue.h>
#include <errno.h>


	//Code taken from man page
               /* List head. */
struct entry 
{
    int socket;
	char username[MAX];
    SLIST_ENTRY(entry) entries;             /* Singly linked List. */
};


//Initalize the head of the Singly linked list
SLIST_HEAD(slisthead, entry);
char servername[MAX];
struct entry *np, *tempNode;
struct slisthead head;  /* Singly linked List head. */
int addUser(int socket, char *username, struct slisthead *head);
void nameLookup(char **string, int sock, struct slisthead *head);
void removeFirstLetter(char *string);
int isUsernameTaken(char *username);
int		portNo;
int main(int argc, char **argv)
{
	//Check for the Command Line Args And process them.
	if (argc < 3)
	{
		fprintf(stderr, "You must include Your server name and the server port when running chatServer\n");
		fprintf(stderr, "Example: ./chatServer2 \"Server1\" 45645\n");
		exit(1);
	}
	else 
	{
		if (strchr(argv[2], ';') != NULL)
		{
		fprintf(stderr, "Server name may not contain the semicolon ';' character! \n");
		exit(1);
		}
		sscanf(argv[2], "%d", &portNo);
		snprintf(servername, MAX, "%s\n", argv[1]);
		servername[MAX-1] = '\0'; 
		//erase the newline character.
		servername[strnlen(servername, MAX) - 1] = '\0';
	}
	SLIST_INIT(&head);      /* Initialize the queue. */
	int				sockfd, newsockfd, maxfd, dirsockfd;
	unsigned int	clilen;
	struct sockaddr_in cli_addr, serv_addr, dir_serv_addr;
	char			s[MAX], un[MAX];
	fd_set			readset, clone;


	/*############### BIND CHATSERVER ###############*/

	/* Add SO_REAUSEADDR option to prevent address in use errors (modified from: "Hands-On Network
	* Programming with C" Van Winkle, 2019. https://learning.oreilly.com/library/view/hands-on-network-programming/9781789349863/5130fe1b-5c8c-42c0-8656-4990bb7baf2e.xhtml */
	int true = 1;

	//setup the chatServer scoket for clients to connect to.
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{
		perror("chatServer: can't open stream socket");
		exit(1);
	}

	/* Add SO_REAUSEADDR option to prevent address in use errors (modified from: "Hands-On Network
	* Programming with C" Van Winkle, 2019. https://learning.oreilly.com/library/view/hands-on-network-programming/9781789349863/5130fe1b-5c8c-42c0-8656-4990bb7baf2e.xhtml */
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&true, sizeof(true)) < 0) 
	{
		perror("chatServer: can't set stream socket address reuse option");
		exit(1);
	}

	//Create the new address for clients to connect to
	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family			= AF_INET;
	serv_addr.sin_addr.s_addr	= htonl(INADDR_ANY);
	serv_addr.sin_port			= htons(portNo);
		
	printf("Binding to chatServer at %s:%d\n", SERV_HOST_ADDR, portNo);


	/* Bind to the new address*/
	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("chatServer: can't bind local address");
		exit(1);
	}

	
	
	/*############### CONNECT DIRSERVER ###############*/

	// Create a new socket for the directoryServer
	if ((dirsockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{
		perror("chatServer: can't open stream socket");
		exit(1);
	}

	/* Add SO_REAUSEADDR option to prevent address in use errors (modified from: "Hands-On Network
	* Programming with C" Van Winkle, 2019. https://learning.oreilly.com/library/view/hands-on-network-programming/9781789349863/5130fe1b-5c8c-42c0-8656-4990bb7baf2e.xhtml */
	if (setsockopt(dirsockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&true, sizeof(true)) < 0) 
	{
		perror("chatServer: can't set stream socket address reuse option");
		exit(1);
	}

	//Setup the address to connect to the directory server
	memset((char *) &dir_serv_addr, 0, sizeof(dir_serv_addr));
	dir_serv_addr.sin_family = AF_INET;
	dir_serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
	dir_serv_addr.sin_port = htons(SERV_TCP_PORT);

	//Connect to the directory Server
	if (connect(dirsockfd, (struct sockaddr *) &dir_serv_addr, sizeof(dir_serv_addr)) < 0) 
	{
		perror("chatServer: can't connect to directory server");
		fprintf(stderr, "Error: %s\n", strerror(errno));
		exit(1);
	}
	printf("Now connected to the directory server\n");

	//Setup socket listener
	listen(sockfd, 5);
	//initialize and set the readset to sockfd
	FD_ZERO(&readset);
	FD_SET(sockfd, &readset);
	//Also need to include dirsockfd now.
	FD_SET(dirsockfd, &readset);
	//Keep count of the maximum file descriptor 
	maxfd = sockfd;
	if (dirsockfd > maxfd)
	{
		maxfd = dirsockfd;
	}
	//Write the ChatServer information and send it to the directoryServer to accept or deny it.
	snprintf(s, MAX, "s%d;%s", portNo, servername);
	write(dirsockfd, s, MAX);

	for (;;) 
	{	
		//Asked Kyle Kohman for help resetting the readset after select and he recommended initializing 
		//a clone of the original readset such that it can be altered without the original being changed.
		clone = readset;
		if (select(maxfd + 1, &clone, NULL, NULL, NULL) > 0)
		{
			//if select shows that there is an incoming message in its result (clone)
			if (FD_ISSET(sockfd, &clone))
			{
				//setup new Client connection
				clilen = sizeof(cli_addr);
				newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
				if (newsockfd < 0) 
				{
					perror("chatServer: accept error");
					exit(1);
				}
				else
				{
					fprintf(stderr, "A new client has connected\n");
					FD_SET(newsockfd, &readset);
					//Since maxfd represents the highest file descriptor, it needs to be updated anytime a fd is added
					if (newsockfd > maxfd) 
					{
						maxfd = newsockfd;
					}
				}
			}

			//For every User:
			for (int i = 0; i <= maxfd; i++)
			{
				//Make sure that the socket isn't that used for new connections, 
				//and make sure that it was selected in the select statement
				if ((i != sockfd) && (FD_ISSET(i, &clone)))
				{
					/* Read the request from the client */
					if (read(i, s, MAX) <= 0) 
					{
						//Reset the un string;
						un[MAX-1] = '\0'; 
						SLIST_FOREACH(np, &head, entries)
						{
							if (np->socket == i) 
							{
								//CHANGED
								//Keep the user's name to be broadcasted later'
								snprintf(un, MAX, np->username);
								un[MAX-1] = '\0';
								//assign the node for freeing later;
								tempNode = np;
							}
						}
						//Remove from the SLIST
						SLIST_REMOVE(&head, tempNode, entry, entries);
						//Free the memory
						free(tempNode);
						if (maxfd == i)
						{
							//update maxfd if appropriate. (avoids seg fault)
							maxfd -= 1;
						}
						//cose the socket
						close(i);
						snprintf(s, MAX, "%s has left the chat room.\n", un);
						fprintf(stderr, "Client %s has discconnected\n", un);

						s[MAX-1] = '\0';
						//remove the connection from the file descriptor
						FD_CLR(i, &readset);
						//Broadcast that the user has left
						SLIST_FOREACH(np, &head, entries)
						{
							write(np->socket, s, MAX);
						}
					}
					else 
					{
						/* Generate an appropriate reply */
						switch(s[0]) 
					{ /* based on the first character of the client's message */

						/* YOUR LOGIC GOES HERE */
						//Case U is for a new user attempting to join.
						case 'x':
						printf("Server name \"%s\" is already taken. Please try again.", servername);
						exit(1);
						break;
						case 'u': 
						//Remove the newline character from entering input
						s[strnlen(s, MAX) - 1] = '\0';
						if (isUsernameTaken(s) == 0)
						{
							int r;
							//if statement for the first user
							if ((r = addUser(i, s, &head)) == 2)
							{
								snprintf(s, MAX, "Welcome!\nYou are the first user to join the chat\n");
								s[MAX-1] = '\0';
							}

							//if statement for the second or higher user
							else if (r == 1)
							{
								un[MAX - 1] = '\0'; 
								char temp[MAX] = {'\0'}; 
								nameLookup(&un, i, &head);
								strncpy(temp, s, MAX); 
								snprintf(s, MAX, "Welcome! %s Joined the chat room.\n", un);
								s[MAX-1] = '\0';
								//Broadcast the message to everyone but the joining user.
								SLIST_FOREACH(np, &head, entries)
								{
									if (np->socket != i)
									{
									 write(np->socket, s, MAX);
									}
								}
							}
						}
						else
						{
							snprintf(s, MAX, "Username is already in use\n");
							s[MAX-1] = '\0';
						}
						write(i, s, MAX);
						break;

						//Case m is for a message
						case 'm': 
						removeFirstLetter(s);
						un[MAX - 1] = '\0'; 
						char temp[MAX - 1] = {'\0'};
						//write the name into un;
						nameLookup(&un, i, &head);
						strncpy(temp, s, MAX); 
						s[MAX-1] = '\0';
						snprintf(s, MAX, "%s: %s", un, temp);
						s[MAX-1] = '\0';
						//Broadcast the message to everyone but the joining user.

						SLIST_FOREACH(np, &head, entries)
						{
							if (np->socket != i)
							{
							 write(np->socket, s, MAX);
							}
						}
						break;

						//Hopefully the default will never hit.
						default: snprintf(s, MAX, "Invalid request\n");
						s[MAX-1] = '\0';
						write(i, s, MAX);
						break;
					}
					memset(&s, 0, MAX);
					}

				}
			}
		}
		else 
		{		
			//Some error has occurred when running Select.
			perror("Error While Selecting");
			exit(1);
		}
	}
	//Close the socket
	close(newsockfd);
}



//SLIST_FOREACH example referenced from https://manpagez.com/man/3/queue/
int isUsernameTaken(char *username)
{
	removeFirstLetter(username);
	int check = 0;
	SLIST_FOREACH(np, &head, entries)
	{
		
		if (strncmp(np->username, username, MAX) == 0) 
		{
			check = 1; 
			break;
		}
	}
	return check;
}


//Referenced https://stackoverflow.com/questions/4295754/how-to-remove-first-character-from-c-string

void removeFirstLetter(char *string)
{
	//Initially I tried using sizeof(string), but it ran into some strange character duplication
	memcpy(string, string + 1, MAX);
	string[MAX - 1] = '\0';
}

//Adds a user to the SLIST
int addUser(int socket, char *username, struct slisthead *h)
{
	//Allocate and assign the new user
	struct entry *newNode = malloc(sizeof(struct entry));
	strncpy(newNode->username, username, MAX);
	newNode->socket = socket;
	//add the user and return 2 if they are the first user (1 if they aren't.)
	if (SLIST_EMPTY(h)) 
	{
        SLIST_INSERT_HEAD(h, newNode, entries);
		//Case 2 means they are the first User
		return 2;
    } else {
		SLIST_INSERT_HEAD(h, newNode, entries);

		return 1;
    }
	printf("ERROR ADDING NODE");
}


//lookup the name of a given socket, and copy it into the passed string memory address.
void nameLookup(char **string, int sock, struct slisthead *h)
{
	SLIST_FOREACH(np, h, entries)
	{
		if (sock == np->socket)
		{
			strncpy(string, np->username, MAX);
		}
	}
}