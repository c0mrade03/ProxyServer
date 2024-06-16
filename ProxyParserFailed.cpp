#include "ProxyParserFailed.h"

#define DEFAULT_NO_HEADERS 8
#define MAX_REQ_LENGTH 65535
#define MIN_REQ_LENGTH 4

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
  int index1;
  int index2;

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

int parsedRequestUnparse(ParsedRequest *request, std::string &buffer, size_t bufferLength)
{
  if (!request || request->buffer.empty())
  {
    return -1;
  }

  size_t temp;
  if (parsedRequestPrintRequestLine(request, buffer, bufferLength, &temp) < 0)
  {
    return -1;
  }

  if (parsedHeaderPrintHeaders(request, buffer, bufferLength - temp) < 0)
  {
    return -1;
  }

  return 0;
}

int parsedRequestUnparseHeaders(ParsedRequest *request, std::string &buffer, size_t bufferLength)
{
  if (!request || request->buffer.empty())
  {
    return -1;
  }

  if (parsedHeaderPrintHeaders(request, buffer, bufferLength) < 0)
  {
    return -1;
  }

  return 0;
}

size_t parsedRequestTotalLength(ParsedRequest *request)
{
  if (request == nullptr || request->buffer.empty())
  {
    return 0;
  }

  return parsedRequestLineLength(request) + parsedHeadersLength(request);
}

int parsedRequestParse(ParsedRequest *parse, std::string &buffer, int bufferLength)
{
  std::string fullAddress;
  std::string savePointer;
  int index;
  std::string currentHeader;
  if (!parse->buffer.empty())
  {
    debug("Parse Object already assigned to a request\n");
    return -1;
  }

  if (bufferLength < MIN_REQ_LENGTH || bufferLength > MAX_REQ_LENGTH)
  {
    debug("Invalid Buffer Length: ", bufferLength);
    return -1;
  }

  std::string tempBuffer = buffer;
  index = tempBuffer.find("\r\n\r\n");
  if (index == std::string::npos)
  {
    debug("Invalid request line, No end of header found");
    return -1;
  }

  index = tempBuffer.find("\r\n");
  parse->buffer = tempBuffer.substr(0, index);
  parse->bufferLength = parse->buffer.size();

  std::istringstream iss(parse->buffer);
  if (!(iss >> parse->method))
  {
    debug("invalid request line, no whitespace\n");
    return -1;
  }
  if (parse->method != "GET")
  {
    debug("invalid request line, method not 'GET': " + parse->method + "\n");
    return -1;
  }

  iss >> fullAddress;
  if (iss.fail())
  {
    debug("invalid request line, no full address\n");
    return -1;
  }
  parse->version = fullAddress.substr(fullAddress.find_last_of(' ') + 1);
  if (parse->version.empty())
  {
    debug("invalid request line, missing version\n");
    return -1;
  }
  if (parse->version.substr(0, 5) != "HTTP/")
  {
    debug("invalid request line, unsupported version " + parse->version + "\n");
    return -1;
  }

  size_t protocolEnd = fullAddress.find("://");
  if (protocolEnd == std::string::npos)
  {
    debug("invalid request line, missing host\n");
    return -1;
  }
  parse->protocol = fullAddress.substr(0, protocolEnd);

  std::string remaining = fullAddress.substr(protocolEnd + 3);
  size_t absUriLen = remaining.length();

  parse->host = remaining.substr(0, remaining.find("/"));
  if (parse->host.empty())
  {
    debug("invalid request line, missing host\n");
    return -1;
  }

  if (parse->host.length() == absUriLen)
  {
    debug("invalid request line, missing absolute path\n");
    return -1;
  }

  parse->path = remaining.substr(parse->host.length());
  if (parse->path.empty())
  {
    parse->path = rootAbsPath;
  }
  else if (parse->path.substr(0, rootAbsPath.size()) == rootAbsPath)
  {
    debug("invalid request line, path cannot begin with two slash characters\n");
    return -1;
  }
  else
  {
    parse->path = rootAbsPath + parse->path;
  }

  size_t colonPos = parse->host.find(':');
  if (colonPos != std::string::npos)
  {
    std::string portString = parse->host.substr(colonPos + 1);
    parse->host = parse->host.substr(0, colonPos);
    if (!portString.empty())
    {
      int port = std::stoi(portString);
      if (port == 0 && errno == EINVAL)
      {
        debug("invalid request line, bad port: " + portString + "\n");
        return -1;
      }
      parse->port = port;
    }
  }

  size_t headerStart = buffer.find("\r\n") + 2;
  size_t headerEnd = buffer.find("\r\n\r\n");
  while (headerStart < headerEnd)
  {
    size_t lineEnd = buffer.find("\r\n", headerStart);
    if (lineEnd == std::string::npos)
      break;
    std::string line = buffer.substr(headerStart, lineEnd - headerStart);
    if (parsedHeaderParse(parse, line))
    {
      return -1;
    }
    headerStart = lineEnd + 2;
  }

  return 0;
}

size_t parsedRequestLineLength(ParsedRequest *request)
{
  if (!request || request->buffer.empty())
  {
    return 0;
  }

  size_t length = request->method.size() + 1 + request->protocol.size() + 3 + request->host.size() + 1 + request->version.size() + 2;
  if (!request->port.empty())
  {
    length += request->port.size() + 1;
  }
  length += request->path.size();
  return length;
}

int parsedRequestPrintRequestLine(ParsedRequest *request, std::string &buffer, size_t bufferLength, size_t *temp)
{
  std::string current = buffer;
  if (bufferLength < parsedRequestLineLength(request))
  {
    debug("Not enough memory for first line\n");
    return -1;
  }
  current += request->method;
  current += " ";
  current += request->protocol;
  current += "://";
  current += request->host;
  if (!request->port.empty())
  {
    current += ":";
    current += request->port;
  }
  current += request->path;
  current += " ";
  current += request->version;
  current += "\r\n";
  *temp = current.size();
  return 0;
}

