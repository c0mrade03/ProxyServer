#pragma once

#include <iostream>
#include <string>
#include <utility>
#include <cerrno>
#include <cctype>

#define DEBUG 1

class ParsedRequest
{
  std::string method;
  std::string protocol;
  std::string host;
  std::string port;
  std::string path;
  std::string buffer;
  size_t bufferLength;
  ParsedHeader *headers;
  size_t headersUsed;
  size_t headersLength;
};

class ParsedHeader
{
  std::string key;
  size_t keyLength;
  std::string value;
  size_t valueLength;
};

ParsedRequest *parsedRequestCreate();

int parsedRequestParse(ParsedRequest *parse, std::string buffer, size_t bufferLength);

void parsedRequestDestroy(ParsedRequest *request);

int parsedRequestUnparse(ParsedRequest *request, std::string buffer, size_t bufferLength);

int parsedRequestUnparseHeaders(ParsedRequest *request, std::string buffer, size_t bufferLength);

size_t parsedRequestTotalLength(ParsedRequest *request);

size_t parsedHeadersLength(ParsedRequest *request);

ParsedHeader *parsedHeaderGet(ParsedRequest *request, std::string key);

int parsedHeaderSet(ParsedRequest *request, std::string key, std::string value);

int parsedHeaderRemove(ParsedRequest *request, std::string key);

void debug(std::string format, ...);

