#include "ProxyParser.h"

#define DEFAULT_NO_HEADERS 8
#define MAX_REQ_LENGTH 65535
#define MIN_REQ_LENGTH 4

static const char *rootAbsPath = "/";

int parsedRequestPrintRequestLine(struct ParsedRequest *request,
                                  char *buffer, size_t bufferLength,
                                  size_t *temp);
size_t parsedRequestLineLength(struct ParsedRequest *request);

void debug(const char *format, ...)
{
  va_list args;
  if (DEBUG)
  {
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
  }
}

int parsedHeaderSet(struct ParsedRequest *request,
                    const char *key, const char *value)
{
  struct ParsedHeader *newHeader;
  parsedHeaderRemove(request, key);

  if (request->headersLength <= request->headersUsed + 1)
  {
    request->headersLength = request->headersLength * 2;
    request->headers =
        (struct ParsedHeader *)realloc(request->headers,
                                       request->headersLength * sizeof(struct ParsedHeader));
    if (!request->headers)
      return -1;
  }

  newHeader = request->headers + request->headersUsed;
  request->headersUsed += 1;

  newHeader->key = (char *)malloc(strlen(key) + 1);
  memcpy(newHeader->key, key, strlen(key));
  newHeader->key[strlen(key)] = '\0';

  newHeader->value = (char *)malloc(strlen(value) + 1);
  memcpy(newHeader->value, value, strlen(value));
  newHeader->value[strlen(value)] = '\0';

  newHeader->keyLength = strlen(key) + 1;
  newHeader->valueLength = strlen(value) + 1;
  return 0;
}

struct ParsedHeader *parsedHeaderGet(struct ParsedRequest *request,
                                     const char *key)
{
  size_t i = 0;
  struct ParsedHeader *temp;
  while (request->headersUsed > i)
  {
    temp = request->headers + i;
    if (temp->key && key && strcmp(temp->key, key) == 0)
    {
      return temp;
    }
    i++;
  }
  return NULL;
}

int parsedHeaderRemove(struct ParsedRequest *request, const char *key)
{
  struct ParsedHeader *temp;
  temp = parsedHeaderGet(request, key);
  if (temp == NULL)
    return -1;

  free(temp->key);
  free(temp->value);
  temp->key = NULL;
  return 0;
}

void parsedHeaderCreate(struct ParsedRequest *request)
{
  request->headers =
      (struct ParsedHeader *)malloc(sizeof(struct ParsedHeader) * DEFAULT_NO_HEADERS);
  request->headersLength = DEFAULT_NO_HEADERS;
  request->headersUsed = 0;
}

size_t parsedHeaderLineLength(struct ParsedHeader *header)
{
  if (header->key != NULL)
  {
    return strlen(header->key) + strlen(header->value) + 4;
  }
  return 0;
}

size_t parsedHeadersLength(struct ParsedRequest *request)
{
  if (!request || !request->buffer)
    return 0;

  size_t i = 0;
  int len = 0;
  while (request->headersUsed > i)
  {
    len += parsedHeaderLineLength(request->headers + i);
    i++;
  }
  len += 2;
  return len;
}

int parsedHeaderPrintHeaders(struct ParsedRequest *request, char *buffer,
                             size_t len)
{
  char *current = buffer;
  struct ParsedHeader *header;
  size_t i = 0;

  if (len < parsedHeadersLength(request))
  {
    debug("buffer for printing headers too small\n");
    return -1;
  }

  while (request->headersUsed > i)
  {
    header = request->headers + i;
    if (header->key)
    {
      memcpy(current, header->key, strlen(header->key));
      memcpy(current + strlen(header->key), ": ", 2);
      memcpy(current + strlen(header->key) + 2, header->value,
             strlen(header->value));
      memcpy(current + strlen(header->key) + 2 + strlen(header->value),
             "\r\n", 2);
      current += strlen(header->key) + strlen(header->value) + 4;
    }
    i++;
  }
  memcpy(current, "\r\n", 2);
  return 0;
}

void parsedHeaderDestroyOne(struct ParsedHeader *header)
{
  if (header->key != NULL)
  {
    free(header->key);
    header->key = NULL;
    free(header->value);
    header->value = NULL;
    header->keyLength = 0;
    header->valueLength = 0;
  }
}

void parsedHeaderDestroy(struct ParsedRequest *request)
{
  size_t i = 0;
  while (request->headersUsed > i)
  {
    parsedHeaderDestroyOne(request->headers + i);
    i++;
  }
  request->headersUsed = 0;

  free(request->headers);
  request->headersLength = 0;
}

int parsedHeaderParse(struct ParsedRequest *request, char *line)
{
  char *key;
  char *value;
  char *index1;
  char *index2;

  index1 = index(line, ':');
  if (index1 == NULL)
  {
    debug("No colon found\n");
    return -1;
  }
  key = (char *)malloc((index1 - line + 1) * sizeof(char));
  memcpy(key, line, index1 - line);
  key[index1 - line] = '\0';

  index1 += 2;
  index2 = strstr(index1, "\r\n");
  value = (char *)malloc((index2 - index1 + 1) * sizeof(char));
  memcpy(value, index1, (index2 - index1));
  value[index2 - index1] = '\0';

  parsedHeaderSet(request, key, value);
  free(key);
  free(value);
  return 0;
}

void parsedRequestDestroy(struct ParsedRequest *request)
{
  if (request->buffer != NULL)
  {
    free(request->buffer);
  }
  if (request->path != NULL)
  {
    free(request->path);
  }
  if (request->headersLength > 0)
  {
    parsedHeaderDestroy(request);
  }
  free(request);
}

struct ParsedRequest *parsedRequestCreate()
{
  struct ParsedRequest *request;
  request = (struct ParsedRequest *)malloc(sizeof(struct ParsedRequest));
  if (request != NULL)
  {
    parsedHeaderCreate(request);
    request->buffer = NULL;
    request->method = NULL;
    request->protocol = NULL;
    request->host = NULL;
    request->path = NULL;
    request->version = NULL;
    request->buffer = NULL;
    request->bufferLength = 0;
  }
  return request;
}

int parsedRequestUnparse(struct ParsedRequest *request, char *buffer,
                         size_t bufferLength)
{
  if (!request || !request->buffer)
    return -1;

  size_t temp;
  if (parsedRequestPrintRequestLine(request, buffer, bufferLength, &temp) < 0)
    return -1;
  if (parsedHeaderPrintHeaders(request, buffer + temp, bufferLength - temp) < 0)
    return -1;
  return 0;
}

int parsedRequestUnparseHeaders(struct ParsedRequest *request, char *buffer,
                                size_t bufferLength)
{
  if (!request || !request->buffer)
    return -1;

  if (parsedHeaderPrintHeaders(request, buffer, bufferLength) < 0)
    return -1;
  return 0;
}

size_t parsedRequestTotalLength(struct ParsedRequest *request)
{
  if (!request || !request->buffer)
    return 0;
  return parsedRequestLineLength(request) + parsedHeadersLength(request);
}

int parsedRequestParse(struct ParsedRequest *parse, const char *buffer,
                       int bufferLength)
{
  char *fullAddress;
  char *saveptr;
  char *index;
  char *currentHeader;

  if (parse->buffer != NULL)
  {
    debug("parse object already assigned to a request\n");
    return -1;
  }

  if (bufferLength < MIN_REQ_LENGTH || bufferLength > MAX_REQ_LENGTH)
  {
    debug("invalid bufferLength %d", bufferLength);
    return -1;
  }

  char *tempBuffer = (char *)malloc(bufferLength + 1);
  memcpy(tempBuffer, buffer, bufferLength);
  tempBuffer[bufferLength] = '\0';

  index = strstr(tempBuffer, "\r\n\r\n");
  if (index == NULL)
  {
    debug("invalid request line, no end of header\n");
    free(tempBuffer);
    return -1;
  }

  index = strstr(tempBuffer, "\r\n");
  if (parse->buffer == NULL)
  {
    parse->buffer = (char *)malloc((index - tempBuffer) + 1);
    parse->bufferLength = (index - tempBuffer) + 1;
  }
  memcpy(parse->buffer, tempBuffer, index - tempBuffer);
  parse->buffer[index - tempBuffer] = '\0';

  parse->method = strtok_r(parse->buffer, " ", &saveptr);
  if (parse->method == NULL)
  {
    debug("invalid request line, no whitespace\n");
    free(tempBuffer);
    free(parse->buffer);
    parse->buffer = NULL;
    return -1;
  }
  if (strcmp(parse->method, "GET"))
  {
    debug("invalid request line, method not 'GET': %s\n",
          parse->method);
    free(tempBuffer);
    free(parse->buffer);
    parse->buffer = NULL;
    return -1;
  }

  fullAddress = strtok_r(NULL, " ", &saveptr);

  if (fullAddress == NULL)
  {
    debug("invalid request line, no full address\n");
    free(tempBuffer);
    free(parse->buffer);
    parse->buffer = NULL;
    return -1;
  }

  parse->version = fullAddress + strlen(fullAddress) + 1;

  if (parse->version == NULL)
  {
    debug("invalid request line, missing version\n");
    free(tempBuffer);
    free(parse->buffer);
    parse->buffer = NULL;
    return -1;
  }
  if (strncmp(parse->version, "HTTP/", 5))
  {
    debug("invalid request line, unsupported version %s\n",
          parse->version);
    free(tempBuffer);
    free(parse->buffer);
    parse->buffer = NULL;
    return -1;
  }

  parse->protocol = strtok_r(fullAddress, "://", &saveptr);
  if (parse->protocol == NULL)
  {
    debug("invalid request line, missing host\n");
    free(tempBuffer);
    free(parse->buffer);
    parse->buffer = NULL;
    return -1;
  }

  const char *rem = fullAddress + strlen(parse->protocol) + strlen("://");
  size_t absUriLen = strlen(rem);

  parse->host = strtok_r(NULL, "/", &saveptr);
  if (parse->host == NULL)
  {
    debug("invalid request line, missing host\n");
    free(tempBuffer);
    free(parse->buffer);
    parse->buffer = NULL;
    return -1;
  }

  if (strlen(parse->host) == absUriLen)
  {
    debug("invalid request line, missing absolute path\n");
    free(tempBuffer);
    free(parse->buffer);
    parse->buffer = NULL;
    return -1;
  }

  parse->path = strtok_r(NULL, " ", &saveptr);
  if (parse->path == NULL)
  {
    int rlen = strlen(rootAbsPath);
    parse->path = (char *)malloc(rlen + 1);
    strncpy(parse->path, rootAbsPath, rlen + 1);
  }
  else if (strncmp(parse->path, rootAbsPath, strlen(rootAbsPath)) == 0)
  {
    debug("invalid request line, path cannot begin "
          "with two slash characters\n");
    free(tempBuffer);
    free(parse->buffer);
    parse->buffer = NULL;
    parse->path = NULL;
    return -1;
  }
  else
  {
    char *tempPath = parse->path;
    int rlen = strlen(rootAbsPath);
    int plen = strlen(parse->path);
    parse->path = (char *)malloc(rlen + plen + 1);
    strncpy(parse->path, rootAbsPath, rlen);
    strncpy(parse->path + rlen, tempPath, plen + 1);
  }

  parse->host = strtok_r(parse->host, ":", &saveptr);
  parse->port = strtok_r(NULL, "/", &saveptr);

  if (parse->host == NULL)
  {
    debug("invalid request line, missing host\n");
    free(tempBuffer);
    free(parse->buffer);
    free(parse->path);
    parse->buffer = NULL;
    parse->path = NULL;
    return -1;
  }

  if (parse->port != NULL)
  {
    int port = strtol(parse->port, (char **)NULL, 10);
    if (port == 0 && errno == EINVAL)
    {
      debug("invalid request line, bad port: %s\n", parse->port);
      free(tempBuffer);
      free(parse->buffer);
      free(parse->path);
      parse->buffer = NULL;
      parse->path = NULL;
      return -1;
    }
  }

  int ret = 0;
  currentHeader = strstr(tempBuffer, "\r\n") + 2;
  while (currentHeader[0] != '\0' &&
         !(currentHeader[0] == '\r' && currentHeader[1] == '\n'))
  {

    if (parsedHeaderParse(parse, currentHeader))
    {
      ret = -1;
      break;
    }

    currentHeader = strstr(currentHeader, "\r\n");
    if (currentHeader == NULL || strlen(currentHeader) < 2)
      break;

    currentHeader += 2;
  }
  free(tempBuffer);
  return ret;
}

size_t parsedRequestLineLength(struct ParsedRequest *request)
{
  if (!request || !request->buffer)
    return 0;

  size_t len =
      strlen(request->method) + 1 + strlen(request->protocol) + 3 +
      strlen(request->host) + 1 + strlen(request->version) + 2;
  if (request->port != NULL)
  {
    len += strlen(request->port) + 1;
  }
  len += strlen(request->path);
  return len;
}

int parsedRequestPrintRequestLine(struct ParsedRequest *request,
                                  char *buffer, size_t bufferLength,
                                  size_t *temp)
{
  char *current = buffer;

  if (bufferLength < parsedRequestLineLength(request))
  {
    debug("not enough memory for first line\n");
    return -1;
  }
  memcpy(current, request->method, strlen(request->method));
  current += strlen(request->method);
  current[0] = ' ';
  current += 1;

  memcpy(current, request->protocol, strlen(request->protocol));
  current += strlen(request->protocol);
  memcpy(current, "://", 3);
  current += 3;
  memcpy(current, request->host, strlen(request->host));
  current += strlen(request->host);
  if (request->port != NULL)
  {
    current[0] = ':';
    current += 1;
    memcpy(current, request->port, strlen(request->port));
    current += strlen(request->port);
  }
  memcpy(current, request->path, strlen(request->path));
  current += strlen(request->path);

  current[0] = ' ';
  current += 1;

  memcpy(current, request->version, strlen(request->version));
  current += strlen(request->version);
  memcpy(current, "\r\n", 2);
  current += 2;
  *temp = current - buffer;
  return 0;
}