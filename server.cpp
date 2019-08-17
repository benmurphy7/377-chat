/*
* echoserverts.c - A concurrent echo server using threads
* and a message buffer.
*/
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>
#include <list>
#include <iostream>
#include <sstream>
#include <iterator>
#include <string>

using namespace std;
/* Simplifies calls to bind(), connect(), and accept() */
typedef struct sockaddr SA;

/* Max text line length */
#define MAXLINE 8192

/* Second argument to listen() */
#define LISTENQ 1024

// We will use this as a simple circular buffer of incoming messages.
char message_buf[20][50];

// This is an index into the message buffer.
int msgi = 0;

// A lock for the message buffer.
pthread_mutex_t lock1;

// Construct and initialize user and rooms.
typedef struct User {
	char* nickname;
	int socket;
	list<char*> blocklist;
	char* room; //Wish we could use a Room object here
}User;

typedef struct Room {
	char* roomname;
	list<User> userlist;
}Room;

list<Room> rooms;
list<User> users;

// Initialize the message buffer to empty strings.
void init_message_buf() {
	int i;
	for (i = 0; i < 20; i++) {
		strcpy(message_buf[i], "");
	}
}

// This function adds a message that was received to the message buffer.
// Notice the lock around the message buffer.
void add_message(char *buf) {
	pthread_mutex_lock(&lock1);
	strncpy(message_buf[msgi % 20], buf, 50);
	int len = strlen(message_buf[msgi % 20]);
	message_buf[msgi % 20][len] = '\0';
	msgi++;
	pthread_mutex_unlock(&lock1);
}

// Destructively modify string to be upper case
void upper_case(char *s) {
	while (*s) {
		*s = toupper(*s);
		s++;
	}
}

// A wrapper around recv to simplify calls.
int receive_message(int connfd, char *message) {
	return recv(connfd, message, MAXLINE, 0);
}

// A wrapper around send to simplify calls.
int send_message(int connfd, char *message) {
	return send(connfd, message, strlen(message), 0);
}

// A predicate function to test incoming message.
int is_list_message(char *message) { return strncmp(message, "-", 1) == 0; }

int send_list_message(int connfd) {
	char message[20 * 50] = "";
	for (int i = 0; i < 20; i++) {
		if (strcmp(message_buf[i], "") == 0) break;
		strcat(message, message_buf[i]);
		strcat(message, ",");
	}

	// End the message with a newline and empty. This will ensure that the
	// bytes are sent out on the wire. Otherwise it will wait for further
	// output bytes.
	strcat(message, "\n\0");
	printf("Sending: %s", message);

	return send_message(connfd, message);
}

int send_echo_message(int connfd, char *message) {
	upper_case(message);
	add_message(message);
	return send_message(connfd, message);
}
void leave_room(User user, Room room)
{
	for (list<User>::iterator it = room.userlist.begin(); it != room.userlist.end(); it++)
	{
		if (it->socket == user.socket)
		{
			 it = room.userlist.erase(it);
			 printf("User removed from room");
			 break;
		}
	}
}
//Count number of spaces in message
int white_spaces(char *message)
{
	int count = 0;
	size_t n = strlen(message);

	for (size_t i = 0; i < n; i++)
	{
		if (isspace(message[i])) ++count;
	}

	return count;
}
//Used to remove user from room userlist (could be callable by a new command)
void kick(User user)
{
	for (list<Room>::iterator it = rooms.begin(); it != rooms.end(); it++)
	{
		if (strcmp(user.room, it->roomname)==0)//added "=0"
		{
			//list<User> userlist = it->userlist;
			for (list<User>::iterator itt = it->userlist.begin(); itt != it->userlist.end(); itt++)
			{
				if (itt->socket == user.socket)
				{
					it->userlist.erase(itt);
					cout << user.nickname << " kicked from " << it->roomname << endl;
                    for (list<User>::iterator iit = users.begin(); iit != users.end(); iit++){
                        if (itt->socket==iit->socket){
                            users.erase(iit);
                            return;
                        }
                    }
				}
			}
		}
	}
}

// Chat Protocol

void join(char* nickname, char* room, int connfd) {
	cout << nickname << " joined " << room << endl;
    for (list<User>::iterator userit = users.begin(); userit != users.end(); userit++){
        if (userit->socket==connfd){
            char errmsg[] ="Leave before join a new room";
            send_message(connfd,errmsg);
            return;
               // leave_room(*userit,*(userit->room);
        }
    }
	for (list<Room>::iterator roomit=rooms.begin();roomit!=rooms.end();roomit++){
	    if (strcmp(roomit->roomname,room)==0){

            //Check if client already made user
            for (list<User>::iterator it = users.begin(); it != users.end(); it++)
            {
                //cout << it->nickname << endl; //This is showing the name the user JUST entered before anything is updated. Why?
                if (strcmp(it->nickname, nickname) == 0)
                {
                    char message[] = "Name already taken."; //Once a client uses \join this will trigger for all other attempts
                    send_message(connfd, message);
                    return;
                }
                //Remove from global user list
                if (it->socket == connfd)
                {
                    //User existing = *it;
                    //it = users.move(it);
                    //Remove user from their room
                    kick(*it);
                }
            }
	    }
	}
	User newuser;
	newuser.nickname = nickname;
	newuser.socket = connfd;
	newuser.room=room;
	bool roomexist = false;
	for (list<Room>::iterator it = rooms.begin(); it != rooms.end(); it++) {
		//cout << it->roomname << endl;
		if (strcmp(it->roomname, room) == 0) {
			//cout << it->roomname << endl;
			//cout << room << endl;
			it->userlist.push_back(newuser);
			roomexist = true;
			break;
		}
	}
	if (roomexist == false) {
		Room newroom;
		newroom.roomname = room;
		//cout << room << endl;
		//cout << newroom.roomname << endl;
		newroom.userlist.push_back(newuser);
		rooms.push_back(newroom);
	}
	users.push_back(newuser);
	string rm(room);
	string confirm = "You have joined room " + rm;
	char * send = new char[confirm.length() + 1];
	strcpy(send, confirm.c_str());
	send_message(connfd, send);

}

void listRoom(int connfd) {
	cout << rooms.size() << endl;
	for (list<Room>::iterator it = rooms.begin(); it != rooms.end(); it++)
	{
		cout << it->roomname << endl;
		string a=" ";
		send_message(connfd, it->roomname);
        send_message(connfd, (char*)a.c_str());
		//send(connfd, it->roomname, strlen(it->roomname), 0);
	}
		
}

void leave(int connfd) {
	//check if this user exists, if "yes", kick the user.
	for (list<User>::iterator it = users.begin(); it != users.end(); it++){
		if (it->socket == connfd){
			kick(*it);
			break;
		}
	}
	char message[] = "Goodbye.";
	send_message(connfd, message);
	shutdown(connfd, SHUT_RDWR);
	close(connfd);
}

void who(int connfd) {
	bool inroom = false;
	list<User> userlist;
	char *roomname;
	string liststring="";
	for (list<User>::iterator it = users.begin(); it != users.end(); it++)
	{
		cout << "Global user: " << it->nickname << endl; //Test to see all users online (all rooms)
		if (it->socket == connfd)
		{
			inroom = true;
			cout<<it->room<<endl;
			roomname = it->room;
		}
	}

	if (inroom)
	{
	    cout<<"in room"<<endl;
		for (list<Room>::iterator it = rooms.begin(); it != rooms.end(); it++)
		{
		    cout<<roomname<<endl;
			if (strcmp(it->roomname, roomname) == 0)
			{
				userlist = it->userlist;
			}
			
		}
	}
	else
	{
		char message[] = "You are not in a room!";
		send_message(connfd, message);
	}
    if (userlist.empty()) cout<<"no user"<<endl;
	for (list<User>::iterator it = userlist.begin(); it != userlist.end(); it++)
	{
		cout << "checkpoint" << endl;
		string any(it->nickname);
        cout << any << endl;
		liststring=liststring+any+" ";

        //cout << liststring<< endl;
		//strcat(liststring," ");
	}
    cout << liststring<< endl;
	send_message(connfd,(char*)liststring.c_str());
}

void help(int connfd) {
	char commands[] = "Available commands:\n\n \\join <nickname> <room>\n \\rooms\n \\leave\n \\who\n \\help\n \\<nickname> <message>";
	send_message(connfd, commands);
}

void sendMsg(char* nickname, char* message, int connfd, int target) {
    //Get name of user sending message
    string line = "";
    bool hasnickname = false;
    //bool hasblocked=false;
    char* sendername;
	string text(message);
    for (list<User>::iterator it = users.begin(); it != users.end(); it++)
    {
        if (it->socket == connfd)
        {
            sendername=it->nickname;
			string name(sendername);
            line = "<From:" + name + "> " + text;
            hasnickname = true;

            break;
        }
    }
    if (!hasnickname)
    {
        char reply[] = "You must have a nickname to send messages.";
        send_message(connfd, reply);
        return;
    }
    bool userfound = false;
    for (list<User>::iterator it = users.begin(); it != users.end(); it++)
    {
        if (strcmp(it->nickname, nickname) == 0)
        {
            userfound = true;
            //send message
            list<char*> blocklist=it->blocklist;
            for (list<char*>::iterator itt=blocklist.begin();itt!=blocklist.end();itt++ ){

                if (strcmp(sendername,*itt)==0){
                    char reply[] = "You have been blocked.";
                    send_message(connfd, reply);
                    return;
                }
            }
			string targetname(nickname);
			string sent = "<To: " + targetname + "> " + text;
            char * send_target = new char[line.length() + 1];
			char * send_sender = new char[sent.length() + 1];
            strcpy(send_target, line.c_str());
			strcpy(send_sender, sent.c_str());
            send_message(target, send_target);
            send_message(connfd, send_sender);
            break;
        }
    }
    if (!userfound)
    {
        char reply[] = "That user does not exist.";
        send_message(connfd, reply);
        return;
        /*string name(it->nickname);
        string text(message);
        string line = name + ": " + text;
        char * send = new char[line.length() + 1];
        strcpy(send, line.c_str());*/
    }
}


void addBlock(int connfd, char* nickname) {

    for (list<User>::iterator it = users.begin(); it != users.end(); it++){
        if (it->socket == connfd){
            it->blocklist.push_back(nickname);
			cout << it->nickname << " joined " << nickname<< endl;
            return;
        }
    }
}

void broadcast(int connfd, char * message)
{
    //Find user sending message
    for (list<User>::iterator it = users.begin(); it != users.end(); it++)
    {
        if (it->socket == connfd)
        {
            //char* charname = new char[strlen(it->nickname) + 1];
            string name(it->nickname);
            string text(message);
            string line = name + ": " + text;
            char * send = new char[line.length() + 1];
            strcpy(send, line.c_str());
            //Get room user is in
            for (list<Room>::iterator itt = rooms.begin(); itt != rooms.end(); itt++)
            {
                if (strcmp(it->room, itt->roomname) == 0)
                {
                    //Send message to all users in room
                    for (list<User>::iterator iit = itt->userlist.begin(); iit != itt->userlist.end(); iit++)
                    {
                        send_message(iit->socket, send);
                    }
                }

            }
        }
    }
}

// Check messages for commands
int process_message(int connfd, char *message) {
	int spaces = white_spaces(message);
	upper_case(message);
	string str(message);

	//Join
	if (str.find("\\JOIN") == 0 && spaces == 2)
	{
		istringstream iss(str);
		vector<string> results((istream_iterator<string>(iss)), istream_iterator<string>());
		string nickname = results.at(1);
		string room = results.at(2);

		char *name = &nickname[0u];
		char *rm = &room[0u];
		//cout << results.at(1) << endl;
		//cout << results.at(2) << endl;
		/*char * pch;

		pch = strtok(message, " ");
		pch = strtok(NULL, " ");
		char * nickname = pch;

		pch = strtok(NULL, " ");
		char *room = pch;*/

		join(name,rm, connfd);
		
		printf("Valid join command\n");
	}
	//Rooms
	else if (str.find("\\ROOMS") == 0 && spaces == 0)
	{
		listRoom(connfd);
		printf("Valid rooms command\n");
	}
	//Leave
	else if (str.find("\\LEAVE") == 0 && spaces == 0)
	{
		leave(connfd);
		printf("Valid leave command\n");
	}
	//Who
	else if (str.find("\\WHO") == 0 && spaces == 0)
	{
		who(connfd);
		printf("Valid who command\n");
	}
	//Help
	else if (str.find("\\HELP") == 0 && spaces == 0)
	{
		help(connfd);
		printf("Valid help command\n");
	}
	//nickname message
	else if (str.find("\\NICKNAME") == 0)
	{
		printf("Valid message command\n");
	}
	//add blocklist
    else if (str.find("\\BLOCK") == 0&& spaces == 1)
    {
        istringstream iss(str);
        vector<string> results((istream_iterator<string>(iss)), istream_iterator<string>());
        string nickname = results.at(1);
        char *name = &nickname[0u];
        addBlock(connfd,name);
        printf("Valid block command\n");
    }
	//Invalid command
	else if (str.find("\\") == 0) {
		//Possible message
		if (spaces > 0) {
			cout << "checking message" << endl;
			string msg(message);
			istringstream iss(msg);
			vector<string> results((istream_iterator<string>(iss)), istream_iterator<string>());
			string nickname = results.at(0);
			nickname.erase(0, 1);
			char *name = &nickname[0u];
			cout << "Target name: " << name << endl;
			string line = "";
			int target;
			for (list<User>::iterator it = users.begin(); it != users.end(); it++) {
				cout << name << " check name " << it->nickname << endl;
				if (strcmp(it->nickname, name) == 0) {
					target = it->socket;
					cout << "user exits" << endl;
					//valid user
					for (int i = 1; i < results.size(); i++) {
						line = line + " " + results.at(i);
					}
					char *msg = &line[0u];
					sendMsg(name, msg, connfd,target);
					return 1;
				}
			}

		}
			//Invalid command
			string reply = "\"" + str + "\" command not recognized.";
			char *send = new char[reply.length() + 1];
			strcpy(send, reply.c_str());
			send_message(connfd, send);
	
	}
		//Chat text
	else
	{
        broadcast(connfd, message);
	}
	/*if (is_list_message(message)) {
		printf("Server responding with list response.\n");
		return send_list_message(connfd);
	}
	else {
		printf("Server responding with echo response.\n");
		return send_echo_message(connfd, message);
	}*/
}

// The main function that each thread will execute.
void echo(int connfd) {
	size_t n;

	// Holds the received message.
	char message[MAXLINE];

	while ((n = receive_message(connfd, message)) > 0) {
		if (n == -1) //Loop will get stuck if n = -1 for some reason
		{
			break;
		}
		message[n] = '\0';  // null terminate message (for string operations)
		printf("Server received message %s (%d bytes)\n", message, (int)n);
		n = process_message(connfd, message);
	}
}

// Helper function to establish an open listening socket on given port.
int open_listenfd(int port) {
	int listenfd;    // the listening file descriptor.
	int optval = 1;  //
	struct sockaddr_in serveraddr;

	/* Create a socket descriptor */
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

	/* Eliminates "Address already in use" error from bind */
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
		sizeof(int)) < 0)
		return -1;

	/* Listenfd will be an endpoint for all requests to port
	on any IP address for this host */
	bzero((char *)&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons((unsigned short)port);
	if (::bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0) return -1;

	/* Make it a listening socket ready to accept connection requests */
	if (listen(listenfd, LISTENQ) < 0) return -1;
	return listenfd;
}

void *thread(void *vargp);

// thread function prototype as we have a forward reference in main.
int main(int argc, char **argv) {
	// Check the program arguments and print usage if necessary.
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	// initialize message buffer.
	init_message_buf();

	// Initialize the message buffer lock.
	pthread_mutex_init(&lock1, NULL);

	// The port number for this server.
	int port = atoi(argv[1]);

	// The listening file descriptor.
	int listenfd = open_listenfd(port);

	// The main server loop - runs forever...
	while (1) {
		// The connection file descriptor.
		int *connfdp = (int*)malloc(sizeof(int));

		// The client's IP address information.
		struct sockaddr_in clientaddr;

		// Wait for incoming connections.
		socklen_t clientlen = sizeof(struct sockaddr_in);
		*connfdp = accept(listenfd, (SA *)&clientaddr, &clientlen);

		/* determine the domain name and IP address of the client */
		struct hostent *hp =
			gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
				sizeof(clientaddr.sin_addr.s_addr), AF_INET);

		// The server IP address information.
		char *haddrp = inet_ntoa(clientaddr.sin_addr);

		// The client's port number.
		unsigned short client_port = ntohs(clientaddr.sin_port);

		printf("server connected to %s (%s), port %u\n", hp->h_name, haddrp,
			client_port);

		// Create a new thread to handle the connection.
		pthread_t tid;
		pthread_create(&tid, NULL, thread, connfdp);
	}
}

/* thread routine */
void *thread(void *vargp) {
	// Grab the connection file descriptor.
	int connfd = *((int *)vargp);
	// Detach the thread to self reap.
	pthread_detach(pthread_self());
	// Free the incoming argument - allocated in the main thread.
	free(vargp);
	// Handle the echo client requests.
	echo(connfd);
	printf("client disconnected.\n");
	// Don't forget to close the connection!
	close(connfd);
	return NULL;
}