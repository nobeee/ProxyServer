// AUTHOR: Ray Powers
// DATE: February 12, 2013
// FILE: ProxyServer.cpp

// DESCRIPTION: This program will server as a caching proxy for multiple clients.

#include<iostream>
#include<sstream>
#include<string>
#include<cstring>
#include<iomanip>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<netdb.h>
#include<pthread.h>

using namespace std;

// Data Structures
struct sockAddr {
  unsigned short sa_family;   // Address family (AF_INET)
  char sa_data[14];           // Protocol-specific addressinfo
};

struct in_address {
  unsigned long s_addr;       // Internet Address (32bits)
};

struct sockAddress_in {
  unsigned short sin_family;  // Address family (AF_INET)
  unsigned short sin_port;    // Port (16bits)
  struct in_addr sin_addr;    // Internet address structure
  char sin_zero[8];           // Not Used.
};

struct threadArgs {
	int clientSock;
};

// TODO - CACHE OBJECT HERE

// Globals
const int MAXPENDING = 40;
const int c_UserNameIndex = 1;
unsigned short serverPort; // up to range 12099 for my own, or 17777
pthread_mutex_t cacheLock;
int status = pthread_mutex_init(&cacheLock, NULL);


// Function Prototypes
void* clientThread(void* args_p);
// Function allows program to handle multiple threads. 
// pre: args_p should carry a socket.
// post: thread will detach and self terminate.

void runServerRequest (int clientSock);
// Function handles server interactions and displays messages from server.
// pre: socket must exist and be open.
// post: none

int main(int argNum, char* argValues[]) {

  // Local Variables
 

  // TODO - Initialize Cache System HERE


  // Need to grab Command-line arguments and convert them to useful types
  // Initialize arguments with proper variables.
  if (argNum != 2){
    // Incorrect number of arguments
    cerr << "Incorrect number of arguments. Please try again." << endl;
    return -1;
  }

  // Need to store arguments
  serverPort = atoi(argValues[1]);

  // Create socket connection
  int conn_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (conn_socket < 0){
    cerr << "Error with socket." << endl;
    exit(-1);
  }

  // Set the socket Fields
  struct sockaddr_in serverAddress;
  serverAddress.sin_family = AF_INET;    // Always AF_INET
  serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
  serverAddress.sin_port = htons(serverPort);
  
  // Assign Port to socket
  int sock_status = bind(conn_socket, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
  if (sock_status < 0) {
    cerr << "Error with bind." << endl;
    exit(-1);
  }

  // Set socket to listen.
  int listen_status = listen(conn_socket, MAXPENDING);
  if (listen_status < 0) {
    cerr << "Error with listening." << endl;
    exit(-1);
  }
  cout << endl << endl << "SERVER: Ready to accept connections. " << endl;
  
    // Accept connections
  while (true) {
    // Accept connections
    struct sockaddr_in clientAddress;
    socklen_t addrLen = sizeof(clientAddress);
    int clientSocket = accept(conn_socket, (struct sockaddr*) &clientAddress, &addrLen);
    if (clientSocket < 0) {
      cerr << "Error accepting connections." << endl;
      exit(-1);
    }

    // Create child thread to handle process
    struct threadArgs* args_p = new threadArgs;
    args_p -> clientSock = clientSocket;
    pthread_t tid;
    int threadStatus = pthread_create(&tid, NULL, clientThread, (void*)args_p);
    if (threadStatus != 0){
      // Failed to create child thread
      cerr << "Failed to create child process." << endl;
      close(clientSocket);
      pthread_exit(NULL);
    }
    
  }
  
  // Well done!
  return 0;
}


void* clientThread(void* args_p){
  
  // Local Variables
  threadArgs* tmp = (threadArgs*) args_p;
  int clientSock = tmp -> clientSock;
  
  delete tmp;

  // Detach Thread to ensure that resources are deallocated on return.
  pthread_detach(pthread_self());

  // Communicate with Client
  runServerRequest(clientSock);

  // Close Client socket
  close(clientSock);

  // Quit thread
  pthread_exit(NULL);
}




void runServerRequest (int clientSock) {
  
  // Local variables.
  struct hostent* host;
  string hostName = "";
  int status;
  int bytesRecv;
  
  // Begin handling communication with Server.

  // Receive HTTP Message from client.
  int bufferSize = 2048;
  int bytesLeft = bufferSize;
  string clientMsg = "";
  char buffer[bufferSize];
  char* buffPTR = buffer;
  while ((bytesRecv = recv(clientSock, buffPTR, bufferSize, 0)) > 0){
    clientMsg.append(buffPTR, bytesRecv);
    if (clientMsg[clientMsg.length()-2] == '\n' && clientMsg[clientMsg.length()-1] == '\n') {
      break;
    }
  }
  cout << clientMsg << endl;
  
  
  // TODO - Forward HTTP Message to host.

  // Get host IP and Set proper fields
 

}

string getHostName(string httpMsg){

  // Local Variables
  string hostName = "";

  // Remove spaces
  for(int i=0; i < httpMsg.length(); i++) {
    if ( httpMsg[i] == ' ') {
      httpMsg.replace(i,1, "");
      i--;
    }
  }

  // Store the whole host name
  hostName.append(httpMsg, 
		  httpMsg.find("Host: ")+6, 
		  httpMsg.find("\n",httpMsg.find("Host: ")) - httpMsg.find("Host: ") - 7);
  cout << hostName << endl;

  // Return Host Name
  return hostName;
}

bool talkToHost(string hostName, string httpMsg){

  // Local Variables
  struct hostent* host;
  struct sockAddress_in serverAddress;
  char* tmpIP;
  unsigned long hostIP;
  int status = 0;
  int hostSock;
  unsigned short hostPort = 80;

  // Get Host IP Address
  host = gethostbyname(hostName.c_str());
  if (!host) {
    cerr << "Unable to resolve hostname's IP Address. Exiting..." << endl;
    return false;
  }
  tmpIP = inet_ntoa(*(struct in_addr *)host ->h_addr_list[0]);
  status = inet_pton(AF_INET, tmpIP, (void*) &hostIP);
  if (status <= 0) exit(-1);
  status = 0;

  // Establish socket and address to talk to Host
  hostSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_addr.s_addr = hostIP;
  serverAddress.sin_port = htons(hostPort);

  // Now we have the right information, let's open a connection to the host.
  status = connect(hostSock, (struct sockaddr *) &serverAddress, sizeof(serverAddress));
  if (status < 0) {
    cerr << "Error opening host connection." << endl;
    exit(-1);
  }

  // Forward Message
  if (!sendMessage(httpMsg, hostSock)){
    exit(-1);
  }
  // Receive Response



  // Great Success!
  return true;
}

bool sendMessage (string messageToSend, int sendSock) {

  // Local Variables
  int msgLength = messageToSend.length();
  int msgSent = 0;
  char msgBuff[msgLength];

  // Transfer message.
  strcpy(msgBuff, messageToSend.c_str());

  // Send message
  if ((msgSent = send(sendSock, msgBuff, msgLength, 0)) != msgLength){
    cerr << "Unable to send message. Aborting connection." << endl;
    return false;
  }
  
  return true;
}

string recvMessage (int recvSock) {

  // Local Variables
  string responseMsg = "";


  return responseMsg;
}
