#include "ProxyParser.h"

#define DEFAULT_NO_HEADERS 8
#define MAX_REQ_LENGTH
#define MIN_REQ_LENGTH

static std::string rootAbsPath = "/";

static void resizeHeader(ParsedHeader *&headers, size_t currentSize, size_t newSize)
{
  ParsedHeader *newHeaders = new ParsedHeader[newSize];
  if (headers)
  {
    std::memcpy(newHeaders, headers, currentSize * sizeof(ParsedHeader));
    delete[] headers;
  }
  headers = newHeaders;
}

int parsedRequestPrintRequestLine(ParsedRequest *request, std::string &buffer, size_t bufferLength, size_t *temp);

size_t parsedRequestLineLength(ParsedRequest *request);

template <typename T>
void debugTemplate(std::ostream &os)
{
  os << std::endl;
}

template <typename T, typename... Args>
void debugTemplate(std::ostream &os, T first, Args... args)
{
  os << first;
  if constexpr (sizeof...(args) > 0)
  {
    os << " ";
  }
  debugTemplate(os, args...);
}

template <typename... Args>
void debug(Args... args)
{
  if (DEBUG)
  {
    debugTemplate(std::cout, args...);
  }
}

int parsedHeaderSet(ParsedRequest *request, std::string &key, std::string &value)
{
  ParsedHeader *newHeader;
  parsedHeaderRemove(request, key);

  if (request->headersLength <= request->headersUsed + 1)
  {
    size_t newHeadersLength = 2 * request->headersLength;
    resizeHeader(request->headers, request->headersLength, newHeadersLength);
    request->headersLength = newHeadersLength;
  }
  if (!request->headers)
  {
    return -1;
  }
  newHeader = &(request->headers[request->headersUsed]);
  request->headersUsed += 1;
  newHeader->key = key;
  newHeader->value = value;
  newHeader->keyLength = key.size();
  newHeader->valueLength = value.size();

  return 0;
}

ParsedHeader *parsedHeaderGet(ParsedRequest *request, std::string &key)
{
  size_t i = 0;
  ParsedHeader *temp;
  while (i < request->headersUsed)
  {
    temp = &(request->headers[i]);
    if (temp->key == key)
    {
      return temp;
    }
    i++;
  }
  return nullptr;
}

int parsedHeaderRemove(ParsedRequest *request, std::string &key)
{
  ParsedHeader *temp = parsedHeaderGet(request, key);
  if (temp == nullptr)
  {
    return -1;
  }

  temp->key.clear();
  temp->value.clear();
  temp->keyLength = 0;
  temp->valueLength = 0;

  return 0;
}

void parsedHeaderCreate(ParsedRequest *request)
{
  request->headers = new ParsedHeader[DEFAULT_NO_HEADERS];
  request->headersLength = DEFAULT_NO_HEADERS;
  request->headersUsed = 0;
}

size_t parsedHeaderLineLength(ParsedHeader *header)
{
  if (header->key.size() > 0)
  {
    return (header->key.size() + header->value.size() + 4);
  }
  return 0;
}

size_t parsedHeadersLength(ParsedRequest *request)
{
  if (request == nullptr || request->buffer.size() == 0)
  {
    return 0;
  }
  size_t i = 0;
  int length = 0;
  while (i < request->headersUsed)
  {
    length += parsedHeaderLineLength(&(request->headers[i]));
    i++;
  }
  length += 2;
  return length;
}

int parsedHeaderPrintHeaders(ParsedRequest *request, std::string &buffer, size_t length)
{
  if (length < parsedHeadersLength(request))
  {
    debug("Buffer for printing headers is too small\n");
    return -1;
  }
  ParsedHeader *header;
  size_t i = 0;
  while (i < request->headersUsed)
  {
    header = &(request->headers[i]);
    if (header->key.size() > 0)
    {
      buffer += header->key;
      buffer += ": ";
      buffer += header->value;
      buffer += "\r\n";
    }
  }
  buffer += "\r\n";
  return 0;
}

void parsedHeaderDestroyOne(ParsedHeader *header)
{
  header->key.clear();
  header->value.clear();
  header->keyLength = 0;
  header->valueLength = 0;
}

void parsedHeaderDestroy(ParsedRequest *request)
{
  size_t i = 0;
  while (i < request->headersUsed)
  {
    parsedHeaderDestroyOne(&(request->headers[i]));
    i++;
  }
  request->headersUsed = 0;
  request->headersLength = 0;
  delete[] request->headers;
  request->headers = nullptr;
}

int parsedHeaderParse(ParsedRequest *request, std::string &line)
{
  std::string key;
  std::string value;
  size_t index1;
  size_t index2;

  index1 = line.find(":");
  if (index1 == std::string::npos)
  {
    debug("No colon found\n");
    return -1;
  }

  key = line.substr(0, index1);
  index1 += 2;
  index2 = line.find("\r\n", index1);
  value = line.substr(index1, index2 - index1);
  parsedHeaderSet(request, key, value);
  return 0;
}

void parsedRequestDestroy(ParsedRequest *request)
{
  if (request->headersLength > 0)
  {
    parsedHeaderDestroy(request);
  }
  delete request;
}

ParsedRequest *parsedRequestCreate()
{
  ParsedRequest *request = new ParsedRequest();
  if (request != nullptr)
  {
    parsedHeaderCreate(request);
    request->method = "";
    request->protocol = "";
    request->host = "";
    request->path = "";
    request->buffer = "";
    request->bufferLength = 0;
  }
  return request;
}

