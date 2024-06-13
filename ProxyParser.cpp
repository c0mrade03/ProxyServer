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

int parsedRequestPrintRequestLine(ParsedRequest *request, std::string buffer, size_t bufferLength, size_t *temp);

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

int parsedHeaderSet(ParsedRequest *request, std::string key, std::string value)
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
  newHeader->key = key;
  newHeader->value = value;
  newHeader->keyLength = key.size();
  newHeader->valueLength = value.size();

  return 0;
}