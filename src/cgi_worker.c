#include <assert.h>
#include <errno.h>
#include <python2.7/Python.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "buffer.h"

static int retrieveFile(char *filename, char *uri) {
  int i;
  for (i = 0; uri[i] != '\0'; ++i) {
    if (uri[i] == '?') {
      return i;
    }
    filename[i] = uri[i];
  }
  return i;
}

static char *env[] = {"CONTENT_LENGTH",
                      "CONTENT_TYPE",
                      "QUERY_STRING",
                      "REMOTE_ADDR",
                      "REMOTE_HOST",
                      "REQUEST_METHOD",
                      "SCRIPT_NAME",
                      "HOST_NAME",
                      "SERVER_PORT",
                      "HTTP_ACCEPT",
                      "HTTP_REFERER",
                      "HTTP_ACCEPT_ENCODING",
                      "HTTP_ACCEPT_LANGUAGE",
                      "HTTP_ACCEPT_CHARSET",
                      "HTTP_COOKIE",
                      "HTTP_USER_AGENT",
                      "HTTP_CONNECTION",
                      "HTTP_HOST",
                      NULL};

static void resetEnvp() {
  for (int i = 0; env[i] != NULL; ++i) {
    setenv(env[i], "", 1);
  }
}

static void setEnv(const char *header_name, const char *header_value) {
  if (strcmp(header_name, "content-type") == 0) {
    setenv("CONTENT_TYPE", header_value, 1);
  } else if (strcmp(header_name, "content-length") == 0) {
    setenv("CONTENT_LENGTH", header_value, 1);
  } else if (strcmp(header_name, "accept") == 0) {
    setenv("HTTP_ACCEPT", header_value, 1);
  } else if (strcmp(header_name, "referer") == 0) {
    setenv("HTTP_REFERER", header_value, 1);
  } else if (strcmp(header_name, "accept-encoding") == 0) {
    setenv("HTTP_ACCEPT_ENCODING", header_value, 1);
  } else if (strcmp(header_name, "accept-language") == 0) {
    setenv("HTTP_ACCEPT_LANGUAGE", header_value, 1);
  } else if (strcmp(header_name, "cookie") == 0) {
    setenv("HTTP_COOKIE", header_value, 1);
  } else if (strcmp(header_name, "user-agent") == 0) {
    setenv("HTTP_USER_AGENT", header_value, 1);
  } else if (strcmp(header_name, "connection") == 0) {
    setenv("HTTP_CONNECTION", header_value, 1);
  } else if (strcmp(header_name, "host") == 0) {
    setenv("HTTP_HOST", header_value, 1);
  } else {
    // fprintf(stderr, "header_name: %s\nheader_value: %s\n", header_name, header_value);
    return;
  }
}
static Buffer buffer;

enum { Method = 0, URI, HeaderName, HeaderValue };
int main() {
  BufferInit(&buffer);
  int state = Method;
  char filename[4096];
  filename[0] = '.';
  char header_name[4096];
  while (1) {
    int err;
    int nread = BufferReadFromFD(&buffer, fileno(stdin), &err);
    if (nread <= 0) {
      fprintf(stderr, "parent exitted, child should exit too\n");
      exit(0);
    }
    int length;
    while ((length = BufferFindString(&buffer)) > 0) {
      char *data = BufferRetrieveData(&buffer, length);
      switch (state) {
        case Method: {
          resetEnvp();
          setenv("REQUEST_METHOD", data, 1);
          ++state;
        } break;
        case URI: {
          memset(filename + 1, 0, 4096 - 1);
          int filename_len = retrieveFile(filename + 1, data);
          setenv("SCRIPT_NAME", filename, 1);
          if (data[filename_len] != 0) {
            setenv("QUERY_STRING", data + filename_len + 1, 1);
          }
          ++state;
        } break;
        case HeaderName: {
          // fprintf(stderr, "header_name: %s\n", data);
          if (length == 1) {
            write(fileno(stdout), &length, sizeof(int));
            // fprintf(stderr, "child receive head ok\n");
            FILE *fp = fopen(filename, "r");
            if (fp == NULL) {
              // fprintf(stderr, "file not found, filename: %s\n", filename);
              char *len_str = getenv("CONTENT_LENGTH");
              int content_length;
              if (len_str[0] != 0 && (content_length = atoi(len_str)) > 0) {
                BufferRetrieveData(&buffer, content_length);
              }
            } else {
              // fprintf(stderr, "file found, name: %s\n", filename);
              Py_Initialize();
              assert(Py_IsInitialized());
              char *argv[] = {filename};
              PySys_SetArgv(1, argv);
              PyRun_SimpleFile(fp, filename);
              Py_Finalize();
              fclose(fp);
            }
            state = Method;
          } else {
            memset(header_name, 0, 4096);
            snprintf(header_name, sizeof(header_name), "%s", data);
            ++state;
          }
        } break;
        case HeaderValue: {
          setEnv(header_name, data);
          state = HeaderName;
        } break;
      }
    }
  }
}
