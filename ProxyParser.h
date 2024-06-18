#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>

#ifndef PROXY_PARSE
#define PROXY_PARSE

#define DEBUG 1

struct ParsedRequest
{
  char *method;
  char *protocol;
  char *host;
  char *port;
  char *path;
  char *version;
  char *buffer;
  size_t bufferLength;
  struct ParsedHeader *headers;
  size_t headersUsed;
  size_t headersLength;
};

struct ParsedHeader
{
  char *key;
  size_t keyLength;
  char *value;
  size_t valueLength;
};

struct ParsedRequest *parsedRequestCreate();

int parsedRequestParse(struct ParsedRequest *parse, const char *buffer, int bufferLength);

void parsedRequestDestroy(struct ParsedRequest *request);

int parsedRequestUnparse(struct ParsedRequest *request, char *buffer, size_t bufferLength);

int parsedRequestUnparseHeaders(struct ParsedRequest *request, char *buffer, size_t bufferLength);

size_t parsedRequestTotalLength(struct ParsedRequest *request);

size_t parsedHeadersLength(struct ParsedRequest *request);

int parsedHeaderSet(struct ParsedRequest *request, const char *key, const char *value);

struct ParsedHeader *parsedHeaderGet(struct ParsedRequest *request, const char *key);

int parsedHeaderRemove(struct ParsedRequest *request, const char *key);

void debug(const char *format, ...);

#endif