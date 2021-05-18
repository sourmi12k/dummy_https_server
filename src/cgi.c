#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "http.h"
#include "log.h"
#include "utils.h"

#define BUFFER_SIZE 4096

static BlockQueue block_queue;
static pthread_t tid;
static pid_t pid;
static int pipe_in, pipe_out;
static char child_code[] = "./cgi_worker";

static char *ENVP[] = {"SERVER_PROTOCOL=HTTP/1.1", "SERVER_SOFTWARE=Liso/1.0", "GATEWAY_INTERFACE=CGI/1.1", NULL};

static void createChild() {
  // create pipes
  int stdin_pipe[2];
  int stdout_pipe[2];
  if (pipe(stdin_pipe) < 0) {
    fprintf(stderr, "Error piping for stdin.\n");
    exit(-1);
  }
  if (pipe(stdout_pipe) < 0) {
    fprintf(stderr, "Error piping for stdout.\n");
    exit(-1);
  }

  // create child process
  pid = fork();
  if (pid < 0) {
    fprintf(stderr, "Something really bad happened when fork()ing.\n");
    exit(-1);
  }

  if (pid == 0) {
    // initialize child
    close(stdout_pipe[0]);
    close(stdin_pipe[1]);
    dup2(stdout_pipe[1], fileno(stdout));
    dup2(stdin_pipe[0], fileno(stdin));

    char *argv[] = {"./cgi_worker", NULL};
    if (execve(child_code, argv, ENVP)) {
      fprintf(stderr, "Error executing execve syscall.\n");
      exit(-1);
    }
  } else {
    close(stdout_pipe[1]);
    close(stdin_pipe[0]);
    pipe_in = stdout_pipe[0];
    int flags = fcntl(pipe_in, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(pipe_in, F_SETFL, flags);

    pipe_out = stdin_pipe[1];
  }
}
static void handle_sigchld(int sig) {
  assert(sig == SIGCHLD);
  LogError("child exitted unexpected, recreate a new child\n");
  close(pipe_in);
  close(pipe_out);
  waitpid(pid, NULL, 0);
  createChild();
}
static int sendToChild(const char *data) {
  int len = strlen(data);
  if (write(pipe_out, data, len + 1) != len + 1) {
    LogError("Error in Sending: %s\n", data);
    return -1;
  }
  return 0;
}
static int waitPipein() {
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(pipe_in, &fds);
  int ret;
  while ((ret = select(pipe_in + 1, &fds, NULL, NULL, NULL)) < 0 && errno == EINTR) {
  }
  if (ret < 0) {
    LogError("select error when waiting on pipe_in\n");
  }
  return ret;
}
static void processRequest(HTTPClient *client) {
  Request *request = &client->request;
  // send request line to child
  if (sendToChild(request->request_line.httpMethod) < 0 || sendToChild(request->request_line.httpUri) < 0) {
    HTTPClientSend(client, NULL, -1);
    return;
  }

  // send request header to child
  struct Header *header = request->list.head->next;
  while (header != NULL) {
    if (sendToChild(header->headerName) < 0 || sendToChild(header->headerValue) < 0) {
      HTTPClientSend(client, NULL, -1);
      return;
    }
    header = header->next;
  }

  // use \0 as end of Head
  if (sendToChild("") < 0) {
    HTTPClientSend(client, NULL, -1);
    return;
  }

  waitPipein();
  int length;
  printf("child receive head ok\n");
  if (read(pipe_in, &length, sizeof(int)) != sizeof(int) || length != 1) {
    HTTPClientSend(client, NULL, -1);
    return;
  }
  // send body to child
  if (request->body_size > 0) {
    if (write(pipe_out, request->body, request->body_size) != request->body_size) {
      HTTPClientSend(client, NULL, -1);
      return;
    }
  }
  printf("body send ok, content-length: %d\n", request->body_size);

  // TODO(sourmi12k): use a better way
  if (waitPipein() < 0) {
    HTTPClientSend(client, NULL, -1);
  }
  char buf[BUFFER_SIZE];
  ssize_t nread;
  // nread == 0 means child exit
  // nread < 0 means pipe is empty
  while ((nread = read(pipe_in, buf, BUFFER_SIZE)) > 0) {
    for (int i = 0; i < nread; ++i) {
      printf("%c", buf[i]);
    }
    HTTPClientSend(client, buf, nread);
  }
  HTTPSendComplete(client);
}
static void *consumer(void *args) {
  DArray tmp;
  DArrayInit(&tmp, 20);
  while (1) {
    BlockQueueWaitAndSwap(&block_queue, &tmp);
    for (int i = 0; i < DArraySize(&tmp); ++i) {
      HTTPClient *client = (HTTPClient *)DArrayGet(&tmp, i);
      processRequest(client);
    }
    DArrayClear(&tmp);
  }
  DArrayFree(&tmp);
  return NULL;
}

void CGIInit(int queue_size) {
  BlockQueueInit(&block_queue, queue_size);
  // create consumer thread
  if (pthread_create(&tid, NULL, consumer, NULL) < 0) {
    fprintf(stderr, "fatal error! cgi thread create failed\n");
    exit(-1);
  }
  signal(SIGCHLD, handle_sigchld);

  createChild();
}
void CGIPush(HTTPClient *client) { BlockQueuePushBack(&block_queue, client); }

void CGIStopAndWait() {
  close(pipe_in);
  close(pipe_out);
  waitpid(pid, NULL, 0);
}
