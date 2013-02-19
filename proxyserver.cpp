// AUTHOR: Ray Powers
// DATE: February 12, 2013
// FILE: ProxyServer.cpp

// DESCRIPTION: This program will serve as a caching proxy for multiple clients.

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
#include<tr1/unordered_map>


using namespace std;

// Data Structures
struct sockAddr {
  unsigned short sa_family; // Address family (AF_INET)
  char sa_data[14]; // Protocol-specific addressinfo
};

struct in_address {
  unsigned long s_addr; // Internet Address (32bits)
};

struct sockAddress_in {
  unsigned short sin_family; // Address family (AF_INET)
  unsigned short sin_port; // Port (16bits)
  struct in_addr sin_addr; // Internet address structure
  char sin_zero[8]; // Not Used.
};

struct threadArgs {
  int clientSock;
};

// Globals
const int MAXPENDING = 10;
unsigned short serverPort; // up to range 12099 for my own, or 17777
pthread_mutex_t cacheLock;
int status = pthread_mutex_init(&cacheLock, NULL);
tr1::unordered_map<string, string> cacheMap;
int numCacheItems = 0;
const int MAXCACHESIZE = 30;


// Function Prototypes
void* clientThread(void* args_p);
// Function allows program to handle multiple threads.
// pre: args_p should carry a socket.
// post: thread will detach and self terminate.

void runServerRequest (int clientSock);
// Function handles server interactions and displays messages from server.
// pre: socket must exist and be open.
// post: none

string getHostName(string httpMsg);
// Function searches message for host informaion.
// pre: none
// post: none

string talkToHost(string hostName, string httpMsg);
// Function initiates communication with a particular host. Returns host response.
// pre: hostName should be resolvable.
// post: returns response string

bool sendMessage (string messageToSend, int sendSock);
// Function sends string to intended socket. Will return false if failure occurs
// pre: socket should exist.
// post: none

string recvMessage (int recvSock);
// Function listens to socket for data. Expects <CRLF CRLF> delimiter for expiration.
// pre: socket should exist.
// post: none

string getResponseCode (string httpMsg);
// Function pulls HTTP response code from message received.
// pre: Message should be a response message.
// post: none

string getTermSig();
// Function returns a typical HTTP control <CRLF>
// pre: none
// post: none

string getURL(string httpMsg);
// Function extracts the URL from a HTTP request.
// pre: none
// post: none

string makeGETrequest(string urlPath);
// Function builds a get request from a proper URL path.
// pre: none
// post: none

string scrubClientMsg (string httpMsg);
// Function removes unecessary Header information
// pre: none
// post: none

void addToCache(string httpRequest, string httpResponse);
// Function adds entry to cache table
// pre: none
// post: none

string checkCache(string httpRequest);
// Function gets value from cache.
// pre: none
// post: none

string cacheCheckingMsg(string httpRequest, string httpResponse);
// Function creates a message to check a cache item's elements.

int main(int argNum, char* argValues[]) {

  // Local Variables
  

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
  serverAddress.sin_family = AF_INET; // Always AF_INET
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
    // Accept connections. No seriously, ACCEPT THEM!
    struct sockaddr_in clientAddress;
    socklen_t addrLen = sizeof(clientAddress);
    int clientSocket = accept(conn_socket, (struct sockaddr*) &clientAddress, &addrLen);
    if (clientSocket < 0) {
      cerr << "Error accepting connections." << endl;
      return 0;
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
  
  // Well done! Your computer managed to finish the infinite loop.
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
  string clientMsg;
  int status;
  int bytesRecv;
  // Begin handling communication with Server.
  // Receive HTTP Message from client.
  clientMsg = recvMessage(clientSock);


  // Forward HTTP Message to host.
  string responseMsg = "";
  // Wasn't in Cache, get proper message from Host
  responseMsg = talkToHost(getHostName(clientMsg), clientMsg);

  // Return HTTP Message to client.
  if (!sendMessage(responseMsg, clientSock)) {
    cerr << "Failed to send message back." << endl;
  }


}

string getHostName(string httpMsg){

  // Local Variables
  string hostName = "";

  // Store the whole host name
  if (httpMsg.find("Host: ") != 0) {
    hostName.append(httpMsg,
		    httpMsg.find("Host: ")+6,
		    httpMsg.find("\n",httpMsg.find("Host: ")) - httpMsg.find("Host: ") - 6);

    // Remove spaces
    for(int i=0; i < hostName.length(); i++) {
      if ( hostName[i] == ' ') {
	hostName.replace(i,1, "");
	i--;
      } else if (hostName[i] == '\r') {
	hostName.replace(i,1, "");
	i--;
      } else if (hostName[i] == '\n') {
	hostName.replace(i,1, "");
	i--;
      }
    }
  }
  // Return Host Name
  return hostName;
}

string talkToHost(string hostName, string httpMsg){

  // Local Variables
  struct hostent* host;
  struct sockAddress_in serverAddress;
  char* tmpIP;
  string responseMsg = "";
  unsigned long hostIP;
  int status = 0;
  int hostSock;
  unsigned short hostPort = 80;

  // Get Host IP Address
  host = gethostbyname(hostName.c_str());
  if (!host) {
    cerr << "Unable to resolve hostname's IP Address. Exiting..." << endl;
    pthread_exit(NULL);
  }
  tmpIP = inet_ntoa(*(struct in_addr *)host ->h_addr_list[0]);
  status = inet_pton(AF_INET, tmpIP, (void*) &hostIP);
  if (status <= 0) pthread_exit(NULL);
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
    pthread_exit(NULL);
  }

  // Forward Message
  if (!sendMessage(httpMsg, hostSock)){
    cerr << "There was an error attempting to send information to host." << endl;
    pthread_exit(NULL);
  }
  // Receive Response
  responseMsg = recvMessage(hostSock);
  if (responseMsg == "") {
    cerr << "Was unable to receive information from host." << endl;
    return responseMsg;
  }

  // Great Success!
  return responseMsg;
}

bool sendMessage (string messageToSend, int sendSock) {

  // Local Variables
  int msgLength = messageToSend.length();
  int msgSent = 0;
  char msgBuff[msgLength-1];

  // Transfer message.
  memcpy(msgBuff, messageToSend.c_str(), msgLength);

  // Send message
  msgSent = send(sendSock, msgBuff, msgLength, 0);
  if (msgSent != msgLength){
    
    cerr << "Unable to send message. Aborting connection." << endl;
    return false;
  }
  
  return true;
}

string recvMessage (int recvSock) {

  // Local Variables
  string responseMsg = "";
  int bytesRecv;
  
  // Begin handling communication with Server.
  int bufferSize = 1024;
  int bytesLeft = bufferSize;
  string clientMsg = "";
  int totalSize = 0;
  char buffer[10000000];
  char* buffPTR = buffer;
  memset(buffPTR, 0, 10000000);
  while (true){
    if (totalSize > 4){
      if (buffer[totalSize-4] == '\r'
	  && buffer[totalSize-3] == '\n'
	  && buffer[totalSize-2] == '\r'
	  && buffer[totalSize-1] == '\n') {
	break;
      }
    }
    bytesRecv = recv(recvSock, (void*) buffPTR, bufferSize, 0);
    if (bytesRecv <= 0) {
      break;
    }
    totalSize += bytesRecv;
    buffPTR += bytesRecv;
    //responseMsg.append(buffPTR, bytesRecv);

  }

  buffer[totalSize] = '\0';
  responseMsg.append(buffer, totalSize);
  return responseMsg;
}

string makeGETrequest(string urlPath) {

  // Local Variables
  string request = "";

  // Build request.
  request.append("GET ");
  request.append(urlPath);
  request.append(" HTTP/1.0\r\n");

  return request;
}

string getURL(string httpMsg) {

  // Local Variables
  string url = "";

  // Process HTTP message
  if (httpMsg.find("https://")){
    url.append(httpMsg,
	       httpMsg.find("GET https://") + 12,
	       httpMsg.find(" HTTP/1.0") - (httpMsg.find("GET https://") + 12));
  } else if (httpMsg.find("http://")) {
    url.append(httpMsg,
	       httpMsg.find("GET http://") + 11,
	       httpMsg.find(" HTTP/1.0") - (httpMsg.find("GET http://") + 11));
  } else {
    url.append(httpMsg,
	       httpMsg.find("GET ") + 4,
	       httpMsg.find(" HTTP/1.0") - (httpMsg.find("GET ") + 4));
  }

  
  return url;
}

string getTermSig() {
  return "\r\n";
}

string getResponseCode (string httpMsg) {
  
  // Local Variables
  string responseCode = "";

  // Store the whole host name
  responseCode.append(httpMsg,
		      httpMsg.find("HTTP/1.0 ")+9,
		      3);

  // Remove spaces
  for(int i=0; i < responseCode.length(); i++) {
    if ( responseCode[i] == ' ') {
      responseCode.replace(i,1, "");
      i--;
    } else if (responseCode[i] == '\r') {
      responseCode.replace(i,1, "");
      i--;
    } else if (responseCode[i] == '\n') {
      responseCode.replace(i,1, "");
      i--;
    }
  }

  // Send it back
  return responseCode;
}

string scrubClientMsg (string httpMsg) {

  // Local variabes
  string betterMsg = "";
  
  betterMsg.append(makeGETrequest(getURL(httpMsg)));

  return betterMsg;
}

void addToCache(string httpRequest, string httpResponse) {
  pthread_mutex_lock(&cacheLock);
  if (numCacheItems != MAXCACHESIZE) {
    cacheMap.insert (make_pair<string, string> (scrubClientMsg(httpRequest), httpResponse));
  } else {
    // Perform RR
    cacheMap.erase(cacheMap.begin());
    cacheMap.insert (make_pair<string, string> (scrubClientMsg(httpRequest), httpResponse));
  }
  pthread_mutex_unlock(&cacheLock);

}

string checkCache(string httpRequest) {
  pthread_mutex_lock(&cacheLock);
  tr1::unordered_map<string,string>::const_iterator foundItem = cacheMap.find(scrubClientMsg(httpRequest));
  if (foundItem != cacheMap.end() ) {
    return foundItem->second;
  } else return "";
  pthread_mutex_unlock(&cacheLock);
}

string cacheCheckingMsg(string httpRequest, string httpResponse) {

  string cacheCheck = "";

  cacheCheck.append(httpRequest);
  cacheCheck.append("If-Modified-Since: ");
  cacheCheck.append(httpResponse,
		    httpResponse.find("Date: ") + 6,
		    httpResponse.find("\n", httpResponse.find("Date: ")) - httpResponse.find("Date: ") + 6);
  cacheCheck.append("\r\n\r\n");
}
