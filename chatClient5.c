#include <stdio.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include "inet.h"
#include "common.h"


int main()
{

	char s[MAX] = {'\0'};
	fd_set			readset;
	int				sockfd, dirsockfd;
	struct sockaddr_in serv_addr, dir_serv_addr;
	int				nread;	/* number of characters */
	int				verified, inServer, awaitInput, port;
	void			getusername();
	int true = 1;

	/*############### CONNECT DIRSERVER ###############*/

	//Create the socket for the directoryServer
	if ((dirsockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
	{
		perror("client: can't open stream socket");
		exit(1);
	}

	/* Add SO_REAUSEADDR option to prevent address in use errors (modified from: "Hands-On Network
	* Programming with C" Van Winkle, 2019. https://learning.oreilly.com/library/view/hands-on-network-programming/9781789349863/5130fe1b-5c8c-42c0-8656-4990bb7baf2e.xhtml */
	if (setsockopt(dirsockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&true, sizeof(true)) < 0) 
	{
		perror("client: can't set stream socket address reuse option");
		exit(1);
	}

	//Setup the address
	memset((char *) &dir_serv_addr, 0, sizeof(dir_serv_addr));
	dir_serv_addr.sin_family = AF_INET;
	dir_serv_addr.sin_addr.s_addr = inet_addr(SERV_HOST_ADDR);
	dir_serv_addr.sin_port = htons(SERV_TCP_PORT);

	
	//printf("Connecting to directory server at %s:%d\n", SERV_HOST_ADDR, SERV_TCP_PORT);

	//Attempt to connect to the directory server
	if (connect(dirsockfd, (struct sockaddr *) &dir_serv_addr, sizeof(dir_serv_addr)) < 0) 
	{
		perror("client: can't connect to directory server");
		fprintf(stderr, "Error: %s\n", strerror(errno));
		exit(1);
	}

	//Alert the server that it is a client connecting.
	snprintf(s, MAX, "c");
	s[MAX-1] = '\0';
	write(dirsockfd, s, MAX);

	inServer = 0;
	awaitInput = 0;
	port = 0;
	//inServer works as a flag to see if the user has entered a valid port and connected. 
	while(!inServer)
	{	
		//reset the socket
		FD_ZERO(&readset);
		FD_SET(dirsockfd, &readset);
		FD_SET(STDIN_FILENO, &readset);

		if ((select(dirsockfd+1, &readset, NULL, NULL, NULL) < 0) || (inServer != 0))
		{
			//Check if the Client has prompted user for input
			if (awaitInput == 1)
			{
				char temp[MAX - 1];

				//Read in the User Input and handle errors accordingly.
				if (fgets(s,sizeof(s),stdin) != NULL)		// [FIX] Use SSCANF instead of fgets
				{
					char *endOfString;
					int n;
					//if the 
					sscanf(s, "%d", &n);
					if (n == NULL)
					{
						printf("No number input found. Goodbye\n");
						exit(1);
					}
					else
					{
						printf("Port Specified: %d\n", n);
						port = n;
						awaitInput = 0;
						inServer = 1;
						break;
					}
				}
			}
		}
		else  
		{
			/* Check whether there's a message from the server to read */
			if (FD_ISSET(dirsockfd, &readset)) 
			{
				//if the buffer is empty
				if ((nread = read(dirsockfd, s, MAX)) <= 0) 
				{
					printf("Error reading from Directory server\n");
				} 
				else 
				{
					//The input is alread read in the if statement
					
					s[MAX-1] = '\0';
					switch(s[0])  
					{

					//There are no servers online. Disconnect.
					case 'N':
						printf("%s", s);
						exit(1);
						break;

					//printing out the servers found in directoryServer
					case '[':
					printf("%s\n", s);
					break;
					
					//The last server option has been sent, so prompt the user and expect some input
					case 'w':
					printf("\n===============================\nEnter a Port # to connect to: ");
					//We have all of the active servers, so we dont need to keep the dirserver online.
					close(dirsockfd);
					awaitInput = 1;
					FD_CLR(dirsockfd, &readset);
					break;

					default: printf("ERROR READING DIRSERVER INPUT\n");
					}
				}
			}
			else
			{
			snprintf(s, MAX, "c");
			s[MAX-1] = '\0';
			write(dirsockfd, s, MAX);
			}
		}
	}

	//At this point, the client should have provided a port to connect to.
	if (port != 0)
	{
		/*############### CONNECT CHATSERVER ###############*/

		//Setup the new Socket
		if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
		{
			perror("client: can't open stream socket");
			exit(1);
		}

		/* Add SO_REAUSEADDR option to prevent address in use errors (modified from: "Hands-On Network
		* Programming with C" Van Winkle, 2019. https://learning.oreilly.com/library/view/hands-on-network-programming/9781789349863/5130fe1b-5c8c-42c0-8656-4990bb7baf2e.xhtml */
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&true, sizeof(true)) < 0) 
		{
			perror("client: can't set stream socket address reuse option");
			exit(1);
		}

		//Setup the address
		memset((char *) &serv_addr, 0, sizeof(serv_addr));
		serv_addr.sin_family = AF_INET;
		serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		serv_addr.sin_port = htons(port);

		
		//connect to the chatserver
		if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		{
			perror("client: can't connect to chat server");
			exit(1);
		}
		else
		{
			printf("Successfully connected to %s:%d\n", SERV_HOST_ADDR, port);
			getusername();
		}
	}
	else 
	{
		printf("AN ERROR OCCURRED BETWEEN DIRECTORY SERVER DISCONNECTION AND CHAT SERVER CONNECTION.\n");
		exit(1);
	}

	//Resume processing client messages as in the previous assignment.
	verified = 0;

	for(;;) {
		
		FD_ZERO(&readset);
		FD_SET(sockfd, &readset);
		FD_SET(STDIN_FILENO, &readset);


		///putting communication logic in the select seemed to lock and make functionality inconsistent
		if (select(sockfd+1, &readset, NULL, NULL, NULL) < 0)
		{
			//Do Nothing
		}
		else 
		{
			/* Check whether there's user input to read */
			if (FD_ISSET(STDIN_FILENO, &readset)) {
				//check if a valid username has been found
				if (verified == 1) 
				{
					char temp[MAX - 1];
					// multiword string fgets example found on https://stackoverflow.com/questions/3555108/multiple-word-string-input-through-scanf
					if (fgets(temp,sizeof(temp),stdin) != NULL) 		// [FIX] Use SSCANF instead of fgets
					{
						snprintf(s, MAX, "m%s", temp);
						s[MAX-1] = '\0';
						write(sockfd, s, MAX);
					}
				} 
				//If no valid username, keep prompting user for input until there is
				else if (verified == 0)
				{
					char temp[MAX - 1];
					if (fgets(temp,sizeof(temp),stdin) != NULL)			// [FIX] Use SSCANF instead of fgets
					{
						snprintf(s, MAX, "u%s", temp);
						s[MAX-1] = '\0';
						write(sockfd, s, MAX);
					}
				} 
				else 
				{
					printf("Error reading or parsing user input\n");
				}
			}

			/* Check whether there's a message from the server to read */
			if (FD_ISSET(sockfd, &readset)) 
			{
				if ((nread = read(sockfd, s, MAX)) <= 0) {
					printf("Error reading from server\n"); // [FIX] Might as well exit
				} else {
					if (s[0] == 'W')
					{
					verified = 1;
					}
					s[MAX-1] = '\0';
					printf("%s", s);
				}
			}
		}
	}
	close(sockfd);
}


//Print statement for the Client to provide input
void getusername()
{
	printf("==============================================================================\n");
	printf("Welcome! Please Enter a Username (Usernames Containing Spaces will be cut off)\n");
	printf("==============================================================================\n");
}

