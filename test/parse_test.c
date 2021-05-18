#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include "http.h"

HTTPClient *client;
Buffer ibuffer;
char *correct_response;
char tmp[4096];

enum { GET = 0, HEAD, POST };
char sample_good_requests[3][4096] = {
    "GET /test.txt HTTP/1.1\r\nHost: www.cs.cmu.edu\r\n\r\n", "HEAD /test.txt HTTP/1.1\r\nHost: www.cs.cmu.edu\r\n\r\n",
    "POST /test.txt HTTP/1.1\r\nHost: www.cs.cmu.edu\r\nContent-length: 20\r\n\r\n01234567890123456789"};
char response200_with_content[] = "HTTP/1.1 200 OK\r\nServer: Liso\r\nContent-length: 20\r\n\r\n01234567890123456789";
char response200[] = "HTTP/1.1 200 OK\r\nServer: Liso\r\n\r\n";
char response400[] = "HTTP/1.1 400 Bad Request\r\nServer: Liso\r\nConnection: Close\r\n\r\n";
char response404[] = "HTTP/1.1 404 Not Found\r\nServer: Liso\r\nConnection: Close\r\n\r\n";
char response408[] = "HTTP/1.1 408 Request Timeout\r\nServer: Liso\r\nConnection: Close\r\n\r\n";
char response411[] = "HTTP/1.1 411 Length Required\r\nServer: Liso\r\nConnection: Close\r\n\r\n";
char response500[] = "HTTP/1.1 500 Internal Error\r\nServer: Liso\r\nConnection: Close\r\n\r\n";
char response501[] = "HTTP/1.1 501 Not Implemented\r\nServer: Liso\r\nConnection: Close\r\n\r\n";
char response505[] = "HTTP/1.1 505 HTTP Version Not Supported\r\nServer: Liso\r\nConnection: Close\r\n\r\n";

void connSend(void *conn, const char *data, int size) {
  strncpy(tmp, data, size);
  tmp[size] = 0;
  if (strcmp(tmp, correct_response) != 0) {
    printf("\n%s", tmp);
    printf("%s", correct_response);
    assert(0);
  }
}

char *correct_responses[128];
void clearCorrectResponses() {
  for (int i = 0; i < 128; ++i) {
    correct_responses[i] = NULL;
  }
}
int pos;
void connSend2(void *conn, const char *data, int size) {
  strncpy(tmp, data, size);
  tmp[size] = 0;
  char *response = correct_responses[pos++];
  if (strcmp(response, tmp) != 0) {
    printf("\n%s", tmp);
    printf("%s", response);
    assert(0);
  }
}

void get_head_helper(int method) {
  char method_names[2][10] = {"GET", "HEAD"};
  printf("\n%s tests\n", method_names[method]);
  client = HTTPClientNew(NULL, connSend, NULL);
  BufferInit(&ibuffer);
  char *request = sample_good_requests[method];
  int len = strlen(request);

  printf("basic %s\n", method_names[method]);
  correct_response = response404;
  BufferAppend(&ibuffer, request, len);
  HTTPClientHandleMessage(client, &ibuffer);
  assert(BufferGetSize(&ibuffer) == 0);

  printf("%s requests splitted into pieces\n", method_names[method]);
  correct_response = response404;
  for (int i = 0; i < 200; ++i) {
    srand((int)time(NULL));
    int first_part = rand() % len;
    BufferAppend(&ibuffer, request, first_part + 1);
    HTTPClientHandleMessage(client, &ibuffer);
    BufferAppend(&ibuffer, request + first_part + 1, len - first_part - 1);
    HTTPClientHandleMessage(client, &ibuffer);
    assert(BufferGetSize(&ibuffer) == 0);
  }

  printf("POST a file and %s it multiple times\n", method_names[method]);
  pos = 0;
  clearCorrectResponses();
  char *responses[2] = {response200_with_content, response200};
  correct_responses[0] = response200;
  for (int i = 0; i < 20; ++i) {
    correct_responses[i] = responses[method];
  }
  correct_response = response200;
  BufferAppend(&ibuffer, sample_good_requests[POST], strlen(sample_good_requests[POST]));
  HTTPClientHandleMessage(client, &ibuffer);
  assert(BufferGetSize(&ibuffer) == 0);

  client->send_response = connSend2;
  for (int i = 0; i < 20; ++i) {
    BufferAppend(&ibuffer, request, len);
  }
  HTTPClientHandleMessage(client, &ibuffer);
  assert(BufferGetSize(&ibuffer) == 0);

  pos = 0;
  clearCorrectResponses();
  client->send_response = connSend;
  BufferFree(&ibuffer);
  HTTPClientFree(client);
  assert(!remove("www/test.txt"));
}

void get_test() { get_head_helper(GET); }

void head_test() { get_head_helper(HEAD); }

char sample_bad_requests[4][4096] = {
    // unnecesary CRLF
    "GET\r\n /test.txt HTTP/1.1\r\nHost: www.cs.cmu.edu\r\n\r\n",
    // request line after a request head
    "GET /test.txt HTTP/1.1\r\nHost: www.cs.cmu.edu\r\nHEAD /test.txt HTTP/1.1\r\n",
    // missing a part in request line
    "GET /test.txt\r\nHost: www.cs.cmu.edu\r\n\r\n",
    // missing a part in request head
    "HEAD /test.txt HTTP/1.1\r\n www.cs.cmu.edu\r\n\r\n"};

char method_not_implemented[] = "GE /test.txt HTTP/1.1\r\nHost: www.cs.cmu.edu\r\n\r\n";
char wrong_http_version[] = "GET /test.txt HTTP/2.1\r\nHost: www.cs.cmu.edu\r\n\r\n";

void error_test() {
  client = HTTPClientNew(NULL, connSend2, NULL);
  BufferInit(&ibuffer);
  char *request = sample_good_requests[GET];
  int len = strlen(request);
  pos = 0;
  clearCorrectResponses();
  printf("\nERROR tests\n");
  printf("bad requests test\n");
  int cnt = 0;
  for (int i = 0; i < 4; ++i) {
    correct_responses[cnt++] = response400;
    correct_responses[cnt++] = response404;
  }
  for (int i = 0; i < 4; ++i) {
    BufferAppend(&ibuffer, sample_bad_requests[i], strlen(sample_bad_requests[i]));
    BufferAppend(&ibuffer, request, len);
    HTTPClientHandleMessage(client, &ibuffer);
    assert(BufferGetSize(&ibuffer) == 0);
  }

  client->send_response = connSend;
  printf("method not implemented test\n");
  correct_response = response501;
  BufferAppend(&ibuffer, method_not_implemented, strlen(method_not_implemented));
  HTTPClientHandleMessage(client, &ibuffer);
  assert(BufferGetSize(&ibuffer) == 0);

  printf("version not supported\n");
  correct_response = response505;
  BufferAppend(&ibuffer, wrong_http_version, strlen(wrong_http_version));
  HTTPClientHandleMessage(client, &ibuffer);
  assert(BufferGetSize(&ibuffer) == 0);

  pos = 0;
  clearCorrectResponses();
  HTTPClientFree(client);
  BufferFree(&ibuffer);
}

int main(int argc, char **argv) {
  get_test();
  head_test();
  error_test();
}