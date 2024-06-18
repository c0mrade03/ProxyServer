#include "ProxyParser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#define MAX_BYTES 4096
#define MAX_CLIENTS 400
#define MAX_SIZE 200 * (1 << 20)
#define MAX_ELEMENT_SIZE 10 * (1 << 20)

typedef struct cacheElement cacheElement;

struct cacheElement
{
  char *data;
  int length;
  char *url;
  time_t lruTimeTrack;
  cacheElement *next;
};

cacheElement *find(char *url);
int addCacheElement(char *data, int size, char *url);
void removeCacheElement();

int portNumber = 8080;
int proxySocketId;
pthread_t tid[MAX_CLIENTS];
sem_t seamaphore;
pthread_mutex_t lock;
cacheElement *head;
int cacheSize;

int sendErrorMessage(int socket, int statusCode)
{
  char str[1024];
  char currentTime[50];
  time_t now = time(0);

  struct tm data = *gmtime(&now);
  strftime(currentTime, sizeof(currentTime), "%a, %d %b %Y %H:%M:%S %Z", &data);

  switch (statusCode)
  {
  case 400:
    snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Rqeuest</H1>\n</BODY></HTML>", currentTime);
    printf("400 Bad Request\n");
    send(socket, str, strlen(str), 0);
    break;

  case 403:
    snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
    printf("403 Forbidden\n");
    send(socket, str, strlen(str), 0);
    break;

  case 404:
    snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
    printf("404 Not Found\n");
    send(socket, str, strlen(str), 0);
    break;

  case 500:
    snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
    send(socket, str, strlen(str), 0);
    break;

  case 501:
    snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
    printf("501 Not Implemented\n");
    send(socket, str, strlen(str), 0);
    break;

  case 505:
    snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
    printf("505 HTTP Version Not Supported\n");
    send(socket, str, strlen(str), 0);
    break;

  default:
    return -1;
  }
  return 1;
}

int connectRemoteServer(char *hostAddress, int portNumber)
{
  int remoteSocket = socket(AF_INET, SOCK_STREAM, 0);

  if (remoteSocket < 0)
  {
    printf("Error in Creating Socket.\n");
    return -1;
  }

  struct hostent *host = gethostbyname(hostAddress);
  if (host == NULL)
  {
    fprintf(stderr, "No such host exists.\n");
    return -1;
  }

  struct sockaddr_in serverAddress;

  bzero((char *)&serverAddress, sizeof(serverAddress));
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(portNumber);

  bcopy((char *)host->h_addr_list[0], (char *)&serverAddress.sin_addr.s_addr, host->h_length);

  if (connect(remoteSocket, (struct sockaddr *)&serverAddress, (socklen_t)sizeof(serverAddress)) < 0)
  {
    fprintf(stderr, "Error in connecting !\n");
    return -1;
  }
  return remoteSocket;
}

int handleRequest(int clientSocket, struct ParsedRequest *request, char *tempRequest)
{
  char *buffer = (char *)malloc(sizeof(char) * MAX_BYTES);
  strcpy(buffer, "GET ");
  strcat(buffer, request->path);
  strcat(buffer, " ");
  strcat(buffer, request->version);
  strcat(buffer, "\r\n");

  size_t length = strlen(buffer);

  if (parsedHeaderSet(request, "Connection", "close") < 0)
  {
    printf("set header key not work\n");
  }

  if (parsedHeaderGet(request, "Host") == NULL)
  {
    if (parsedHeaderSet(request, "Host", request->host) < 0)
    {
      printf("Set \"Host\" header key not working\n");
    }
  }

  if (parsedRequestUnparseHeaders(request, buffer + length, (size_t)MAX_BYTES - length) < 0)
  {
    printf("unparse failed\n");
  }

  int serverPort = 80;
  if (request->port != NULL)
  {
    serverPort = atoi(request->port);
  }

  int remoteSocketId = connectRemoteServer(request->host, serverPort);

  if (remoteSocketId < 0)
  {
    return -1;
  }

  int bytesSend = send(remoteSocketId, buffer, strlen(buffer), 0);

  bzero(buffer, MAX_BYTES);

  bytesSend = recv(remoteSocketId, buffer, MAX_BYTES - 1, 0);
  char *tempBuffer = (char *)malloc(sizeof(char) * MAX_BYTES); // temp buffer
  int tempBufferSize = MAX_BYTES;
  int tempBufferIndex = 0;

  while (bytesSend > 0)
  {
    bytesSend = send(clientSocket, buffer, bytesSend, 0);

    for (long unsigned i = 0; i < bytesSend / sizeof(char); i++)
    {
      tempBuffer[tempBufferIndex] = buffer[i];
      tempBufferIndex++;
    }
    tempBufferSize += MAX_BYTES;
    tempBuffer = (char *)realloc(tempBuffer, tempBufferSize);

    if (bytesSend < 0)
    {
      perror("Error in sending data to client socket.\n");
      break;
    }
    bzero(buffer, MAX_BYTES);

    bytesSend = recv(remoteSocketId, buffer, MAX_BYTES - 1, 0);
  }
  tempBuffer[tempBufferIndex] = '\0';
  free(buffer);
  addCacheElement(tempBuffer, strlen(tempBuffer), tempRequest);
  printf("Done\n");
  free(tempBuffer);

  close(remoteSocketId);
  return 0;
}

int checkHTTPversion(char *message)
{
  int version = -1;

  if (strncmp(message, "HTTP/1.1", 8) == 0)
  {
    version = 1;
  }
  else if (strncmp(message, "HTTP/1.0", 8) == 0)
  {
    version = 1;
  }
  else
    version = -1;

  return version;
}

void *thread_fn(void *socketNew)
{
  sem_wait(&seamaphore);
  int p;
  sem_getvalue(&seamaphore, &p);
  printf("semaphore value:%d\n", p);
  int *t = (int *)(socketNew);
  int socket = *t;
  int bytesSendClient, length;

  char *buffer = (char *)calloc(MAX_BYTES, sizeof(char));

  bzero(buffer, MAX_BYTES);
  bytesSendClient = recv(socket, buffer, MAX_BYTES, 0);

  while (bytesSendClient > 0)
  {
    length = strlen(buffer);
    if (strstr(buffer, "\r\n\r\n") == NULL)
    {
      bytesSendClient = recv(socket, buffer + length, MAX_BYTES - length, 0);
    }
    else
    {
      break;
    }
  }

  char *tempRequest = (char *)malloc(strlen(buffer) * sizeof(char) + 1);
  for (long unsigned i = 0; i < strlen(buffer); i++)
  {
    tempRequest[i] = buffer[i];
  }

  struct cacheElement *temp = find(tempRequest);

  if (temp != NULL)
  {
    int size = temp->length / sizeof(char);
    int pos = 0;
    char response[MAX_BYTES];
    while (pos < size)
    {
      bzero(response, MAX_BYTES);
      for (int i = 0; i < MAX_BYTES; i++)
      {
        response[i] = temp->data[pos];
        pos++;
      }
      send(socket, response, MAX_BYTES, 0);
    }
    printf("Data retrived from the Cache\n\n");
    printf("%s\n\n", response);
  }

  else if (bytesSendClient > 0)
  {
    length = strlen(buffer);
    struct ParsedRequest *request = parsedRequestCreate();

    if (parsedRequestParse(request, buffer, length) < 0)
    {
      printf("Parsing failed\n");
    }
    else
    {
      bzero(buffer, MAX_BYTES);
      if (!strcmp(request->method, "GET"))
      {

        if (request->host && request->path && (checkHTTPversion(request->version) == 1))
        {
          bytesSendClient = handleRequest(socket, request, tempRequest);
          if (bytesSendClient == -1)
          {
            sendErrorMessage(socket, 500);
          }
        }
        else
          sendErrorMessage(socket, 500);
      }
      else
      {
        printf("This code doesn't support any method other than GET\n");
      }
    }
    parsedRequestDestroy(request);
  }

  else if (bytesSendClient < 0)
  {
    perror("Error in receiving from client.\n");
  }
  else if (bytesSendClient == 0)
  {
    printf("Client disconnected!\n");
  }

  shutdown(socket, SHUT_RDWR);
  close(socket);
  free(buffer);
  sem_post(&seamaphore);

  sem_getvalue(&seamaphore, &p);
  printf("Semaphore post value:%d\n", p);
  free(tempRequest);
  return NULL;
}

int main(int argc, char *argv[])
{

  int clientSocketId, clientLength;
  struct sockaddr_in serverAddress, clientAddress;

  sem_init(&seamaphore, 0, MAX_CLIENTS);
  pthread_mutex_init(&lock, NULL);

  if (argc == 2)
  {
    portNumber = atoi(argv[1]);
  }
  else
  {
    printf("Too few arguments\n");
    exit(1);
  }

  printf("Setting Proxy Server Port : %d\n", portNumber);

  proxySocketId = socket(AF_INET, SOCK_STREAM, 0);

  if (proxySocketId < 0)
  {
    perror("Failed to create socket.\n");
    exit(1);
  }

  int reuse = 1;
  if (setsockopt(proxySocketId, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 0)
  {
    perror("setsockopt(SO_REUSEADDR) failed\n");
  }

  bzero((char *)&serverAddress, sizeof(serverAddress));
  serverAddress.sin_family = AF_INET;
  serverAddress.sin_port = htons(portNumber);
  serverAddress.sin_addr.s_addr = INADDR_ANY;

  if (bind(proxySocketId, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
  {
    perror("Port is not free\n");
    exit(1);
  }
  printf("Binding on port: %d\n", portNumber);

  int listenStatus = listen(proxySocketId, MAX_CLIENTS);

  if (listenStatus < 0)
  {
    perror("Error while Listening !\n");
    exit(1);
  }

  int i = 0;
  int connectedSocketId[MAX_CLIENTS];

  while (1)
  {

    bzero((char *)&clientAddress, sizeof(clientAddress));
    clientLength = sizeof(clientAddress);

    clientSocketId = accept(proxySocketId, (struct sockaddr *)&clientAddress, (socklen_t *)&clientLength);
    if (clientSocketId < 0)
    {
      fprintf(stderr, "Error in Accepting connection !\n");
      exit(1);
    }
    else
    {
      connectedSocketId[i] = clientSocketId;
    }

    struct sockaddr_in *clientPointer = (struct sockaddr_in *)&clientAddress;
    struct in_addr ipAddress = clientPointer->sin_addr;
    char str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ipAddress, str, INET_ADDRSTRLEN);
    printf("Client is connected with port number: %d and ip address: %s \n", ntohs(clientAddress.sin_port), str);
    pthread_create(&tid[i], NULL, thread_fn, (void *)&connectedSocketId[i]);
    i++;
  }
  close(proxySocketId); // Close socket
  return 0;
}

cacheElement *find(char *url)
{
  cacheElement *site = NULL;
  int tempLockValue = pthread_mutex_lock(&lock);
  printf("Remove Cache Lock Acquired %d\n", tempLockValue);
  if (head != NULL)
  {
    site = head;
    while (site != NULL)
    {
      if (!strcmp(site->url, url))
      {
        printf("LRU Time Track Before : %ld", site->lruTimeTrack);
        printf("\nurl found\n");
        site->lruTimeTrack = time(NULL);
        printf("LRU Time Track After : %ld", site->lruTimeTrack);
        break;
      }
      site = site->next;
    }
  }
  else
  {
    printf("\nurl not found\n");
  }
  tempLockValue = pthread_mutex_unlock(&lock);
  printf("Remove Cache Lock Unlocked %d\n", tempLockValue);
  return site;
}

void removeCacheElement()
{
  cacheElement *p;
  cacheElement *q;
  cacheElement *temp;
  int tempLockValue = pthread_mutex_lock(&lock);
  printf("Remove Cache Lock Acquired %d\n", tempLockValue);
  if (head != NULL)
  {
    for (q = head, p = head, temp = head; q->next != NULL; q = q->next)
    {
      if (((q->next)->lruTimeTrack) < (temp->lruTimeTrack))
      {
        temp = q->next;
        p = q;
      }
    }
    if (temp == head)
    {
      head = head->next;
    }
    else
    {
      p->next = temp->next;
    }
    cacheSize = cacheSize - (temp->length) - sizeof(cacheElement) - strlen(temp->url) - 1;
    free(temp->data);
    free(temp->url);
    free(temp);
  }
  tempLockValue = pthread_mutex_unlock(&lock);
  printf("Remove Cache Lock Unlocked %d\n", tempLockValue);
}

int addCacheElement(char *data, int size, char *url)
{
  int tempLockValue = pthread_mutex_lock(&lock);
  printf("Add Cache Lock Acquired %d\n", tempLockValue);
  int elementSize = size + 1 + strlen(url) + sizeof(cacheElement);
  if (elementSize > MAX_ELEMENT_SIZE)
  {
    tempLockValue = pthread_mutex_unlock(&lock);
    printf("Add Cache Lock Unlocked %d\n", tempLockValue);
    return 0;
  }
  else
  {
    while (cacheSize + elementSize > MAX_SIZE)
    {
      removeCacheElement();
    }
    cacheElement *element = (cacheElement *)malloc(sizeof(cacheElement));
    element->data = (char *)malloc(size + 1);
    strcpy(element->data, data);
    element->url = (char *)malloc(1 + (strlen(url) * sizeof(char)));
    strcpy(element->url, url);
    element->lruTimeTrack = time(NULL);
    element->next = head;
    element->length = size;
    head = element;
    cacheSize += elementSize;
    tempLockValue = pthread_mutex_unlock(&lock);
    printf("Add Cache Lock Unlocked %d\n", tempLockValue);
    return 1;
  }
  return 0;
}