#include <arpa/inet.h>
#include <bits/posix1_lim.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <unistd.h>

#include "libhttp.h"
#include "wq.h"

/*
 * Global configuration variables.
 * You need to use these in your implementation of handle_files_request and
 * handle_proxy_request. Their values are set up in main() using the
 * command line arguments (already implemented for you).
 */
wq_t work_queue; // Only used by poolserver
int num_threads; // Only used by poolserver
int server_port; // Default value: 8000
char* server_files_directory;
char* server_proxy_hostname;
int server_proxy_port;
/*(Helper) Send error response*/

void send_error_response(int fd,int status_code){
  http_start_response(fd,status_code);
  http_send_header(fd,"Content-Type","text/html");
  http_end_headers(fd);
}

/*(Helper) Send response headers with files*/
void send_response_headers(int fd,char* path,int status_code,char* size_str){
  http_start_response(fd, status_code);
  http_send_header(fd, "Content-Type", http_get_mime_type(path));
  http_send_header(fd,"Content-Length",size_str);
  http_end_headers(fd);
}

/*(Helper) Get the index path*/
char* get_index_path(char* path){
  char* index_path = malloc(strlen(path) + strlen("/index.html") + 1);
  sprintf(index_path,"%s/index.html",path);
  return index_path;
}


/*
 * Serves the contents the file stored at `path` to the client socket `fd`.
 * It is the caller's reponsibility to ensure that the file stored at `path` exists.
 */
void serve_file(int fd, char* path) {

  /* TODO: PART 2 */
  /* PART 2 BEGIN */
  int file_fd  = open(path,O_RDONLY);
  struct stat st;
  stat(path,&st);

  char size_str[20];
  sprintf(size_str,"%ld",st.st_size);

  send_response_headers(fd, path, 200,size_str);

  char buf[4096];
  ssize_t n;
  while( (n = read(file_fd,buf,sizeof(buf)))>0){
    write(fd,buf,n);
  }

  close(file_fd);

  /* PART 2 END */
}

void serve_directory(int fd, char* path) {
  http_start_response(fd, 200);
  http_send_header(fd, "Content-Type", http_get_mime_type(".html"));
  http_end_headers(fd);

  /* TODO: PART 3 */
  /* PART 3 BEGIN */

  // TODO: Open the directory (Hint: opendir() may be useful here)
  DIR* dir = opendir(path);
  if(dir==NULL){
    fprintf(stderr,"Cannot open directory %s\n",path);
    return;
  }
  /**
   * TODO: For each entry in the directory (Hint: look at the usage of readdir() ),
   * send a string containing a properly formatted HTML. (Hint: the http_format_href()
   * function in libhttp.c may be useful here)
   */
   /* 整理url格式，去掉开头的 ./ */
  char* url_path;
  if(strcmp(path, "./") == 0){
    url_path = "";
  }else{
    url_path = path + 3;
  }

  char clean_path[4096];
  strcpy(clean_path,url_path);
  int len = strlen(clean_path);
  if(len > 0 && clean_path[len -1] == '/'){
    clean_path[len -1] = '\0';
  }
  printf("clean path: %s\n",clean_path);


  struct dirent* entry;
  char buf[4096];

  /*send parent dir link*/
  char parent[4096];
  strcpy(parent,clean_path);
  char* last_slash = strrchr(parent,'/');
  if(strcmp(clean_path,"" )== 0){ // the path is already the root directory
    strcpy(parent,"");
  }else{
    if(last_slash != NULL){
      *last_slash = '\0';
    }else{
      strcpy(parent,"");
    }
  }
  http_format_href(buf,parent,"..");
  write(fd, buf, strlen(buf));
  write(fd, "\n", 1);

  while((entry = readdir(dir))!=NULL){
    if(strcmp(entry->d_name,".")==0 || strcmp(entry->d_name,"..")==0){
      continue;
    }
    char* filename = entry->d_name;

    http_format_href(buf,clean_path,filename);
    write(fd, buf, strlen(buf));
    write(fd,"\n",1);
  }
  closedir(dir);
  /* PART 3 END */
}

/*(Helper) relay the data stream between the sockets*/
static void relay_stream(int src_fd,int dst_fd){
  char buf[4096];
  ssize_t n ;
  while((n = read(src_fd,buf,sizeof(buf)))>0){
    ssize_t total = 0;
    while(total<n){
      ssize_t m = write(dst_fd, buf+total, n-total);
      if(m <= 0){
        return;
      }
      total +=m;
    }
  }
}

/*
 * Reads an HTTP request from client socket (fd), and writes an HTTP response
 * containing:
 *
 *   1) If user requested an existing file, respond with the file
 *   2) If user requested a directory and index.html exists in the directory,
 *      send the index.html file.
 *   3) If user requested a directory and index.html doesn't exist, send a list
 *      of files in the directory with links to each.
 *   4) Send a 404 Not Found response.
 *
 *   Closes the client socket (fd) when finished.
 */
void handle_files_request(int fd) {

  struct http_request* request = http_request_parse(fd);

  if (request == NULL || request->path[0] != '/') {
    send_error_response(fd, 400);
    close(fd);
    return;
  }

  if (strstr(request->path, "..") != NULL) {
    send_error_response(fd, 403);
    close(fd);
    return;
  }

  /* Add `./` to the beginning of the requested path */
  char* path = malloc(2 + strlen(request->path) + 1);
  path[0] = '.';
  path[1] = '/';
  memcpy(path + 2, request->path, strlen(request->path) + 1);

  /*
   * TODO: PART 2 is to serve files. If the file given by `path` exists,
   * call serve_file() on it. Else, serve a 404 Not Found error below.
   * The `stat()` syscall will be useful here.
   *
   * TODO: PART 3 is to serve both files and directories. You will need to
   * determine when to call serve_file() or serve_directory() depending
   * on `path`. Make your edits below here in this function.
   */

  /* PART 2 & 3 BEGIN */
  struct stat st;

  if(stat(path,&st)==0){
    if(S_ISREG(st.st_mode)){
      serve_file(fd,path);
    }else if(S_ISDIR(st.st_mode)){
      /*access the 'index.html' first if it's existed*/
      char* index_path = get_index_path(path);
      struct stat index_st;
      if(stat(index_path, &index_st)==0 && S_ISREG(index_st.st_mode)){
        serve_file(fd,index_path);
      }else{
        serve_directory(fd,path);
      }
      free(index_path);
    }
  }else{
    send_error_response(fd, 404);
  }
  /* PART 2 & 3 END */


  close(fd);
  return;
}

/*
 * Opens a connection to the proxy target (hostname=server_proxy_hostname and
 * port=server_proxy_port) and relays traffic to/from the stream fd and the
 * proxy target_fd. HTTP requests from the client (fd) should be sent to the
 * proxy target (target_fd), and HTTP responses from the proxy target (target_fd)
 * should be sent to the client (fd).
 *
 *   +--------+     +------------+     +--------------+
 *   | client | <-> | httpserver | <-> | proxy target |
 *   +--------+     +------------+     +--------------+
 *
 *   Closes client socket (fd) and proxy target fd (target_fd) when finished.
 */

struct proxy_args{
  int src_fd;
  int dst_fd;
};

// 线程函数,不解析数据，只进行数据转发
void* proxy_data(void* arg){
  struct proxy_args* args = (struct proxy_args*)arg;
  int fd = args->src_fd;
  int target_fd = args->dst_fd;
  free(args);

  relay_stream(fd, target_fd);
  shutdown(fd, SHUT_RDWR);
  shutdown(target_fd,SHUT_RDWR);
  return NULL;

}

void handle_proxy_request(int fd) {

  /*
  * The code below does a DNS lookup of server_proxy_hostname and
  * opens a connection to it. Please do not modify.
  */
  struct sockaddr_in target_address;
  memset(&target_address, 0, sizeof(target_address));
  target_address.sin_family = AF_INET;
  target_address.sin_port = htons(server_proxy_port);

  // Use DNS to resolve the proxy target's IP address
  struct hostent* target_dns_entry = gethostbyname2(server_proxy_hostname, AF_INET);

  // Create an IPv4 TCP socket to communicate with the proxy target.
  int target_fd = socket(PF_INET, SOCK_STREAM, 0);
  if (target_fd == -1) {
    fprintf(stderr, "Failed to create a new socket: error %d: %s\n", errno, strerror(errno));
    close(fd);
    exit(errno);
  }

  if (target_dns_entry == NULL) {
    fprintf(stderr, "Cannot find host: %s\n", server_proxy_hostname);
    close(target_fd);
    close(fd);
    exit(ENXIO);
  }

  char* dns_address = target_dns_entry->h_addr_list[0]; // Take the first resolved IP address for the proxy target.

  // Connect to the proxy target.
  memcpy(&target_address.sin_addr, dns_address, sizeof(target_address.sin_addr));
  int connection_status =
      connect(target_fd, (struct sockaddr*)&target_address, sizeof(target_address));

  if (connection_status < 0) {
    /* Dummy request parsing, just to be compliant. */
    http_request_parse(fd);

    http_start_response(fd, 502);
    http_send_header(fd, "Content-Type", "text/html");
    http_end_headers(fd);

    shutdown(fd, SHUT_RDWR);
    shutdown(target_fd, SHUT_RDWR);
    close(fd);
    close(target_fd);
    return;
  }

  /* TODO: PART 4 */
  /* PART 4 BEGIN */
  pthread_t thread1,thread2;
  struct proxy_args* a1 = malloc(sizeof(struct proxy_args)); // client to proxy target
  struct proxy_args* a2 = malloc(sizeof(struct proxy_args)); // proxy target to client
  if (a1 == NULL || a2 == NULL) {
    free(a1);
    free(a2);
    shutdown(fd, SHUT_RDWR);
    shutdown(target_fd, SHUT_RDWR);
    close(fd);
    close(target_fd);
    return;
  }

  a1->src_fd = fd;
  a1->dst_fd = target_fd;

  a2->src_fd = target_fd;
  a2->dst_fd = fd;

  int thread1_status = pthread_create(&thread1, NULL, proxy_data, a1);
  int thread2_status = pthread_create(&thread2, NULL, proxy_data, a2);

  if (thread1_status != 0 || thread2_status != 0) {
    if (thread1_status != 0) {
      free(a1);
    }
    if (thread2_status != 0) {
      free(a2);
    }
    shutdown(fd, SHUT_RDWR);
    shutdown(target_fd, SHUT_RDWR);
    // one of the thread is successfully created
    if (thread1_status == 0) {
      pthread_join(thread1, NULL);
    }
    if (thread2_status == 0) {
      pthread_join(thread2, NULL);
    }
    close(fd);
    close(target_fd);
    return;
  }
  //all of the threads is successfully created
  pthread_join(thread1,NULL);
  pthread_join(thread2,NULL);

  close(fd);
  close(target_fd);
  /* PART 4 END */
}

#ifdef POOLSERVER
/*
 * All worker threads will run this function until the server shutsdown.
 * Each thread should block until a new request has been received.
 * When the server accepts a new connection, a thread should be dispatched
 * to send a response to the client.
 */
void* handle_clients(void* void_request_handler) {
  void (*request_handler)(int) = (void (*)(int))void_request_handler;
  /* (Valgrind) Detach so thread frees its memory on completion, since we won't
   * be joining on it. */
  pthread_detach(pthread_self());

  /* TODO: PART 7 */
  /* PART 7 BEGIN */

  /* PART 7 END */
}

/*
 * Creates `num_threads` amount of threads. Initializes the work queue.
 */
void init_thread_pool(int num_threads, void (*request_handler)(int)) {

  /* TODO: PART 7 */
  /* PART 7 BEGIN */

  /* PART 7 END */
}
#endif

/*
 * Opens a TCP stream socket on all interfaces with port number PORTNO. Saves
 * the fd number of the server socket in *socket_number. For each accepted
 * connection, calls request_handler with the accepted fd number.
 */
void serve_forever(int* socket_number, void (*request_handler)(int)) {

  struct sockaddr_in server_address, client_address;
  size_t client_address_length = sizeof(client_address);
  int client_socket_number;

  // Creates a socket for IPv4 and TCP.
  *socket_number = socket(PF_INET, SOCK_STREAM, 0);
  if (*socket_number == -1) {
    perror("Failed to create a new socket");
    exit(errno);
  }

  int socket_option = 1;
  if (setsockopt(*socket_number, SOL_SOCKET, SO_REUSEADDR, &socket_option, sizeof(socket_option)) ==
      -1) {
    perror("Failed to set socket options");
    exit(errno);
  }

  // Setup arguments for bind()
  memset(&server_address, 0, sizeof(server_address));
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(server_port);

  /*
   * TODO: PART 1
   *
   * Given the socket created above, call bind() to give it
   * an address and a port. Then, call listen() with the socket.
   * An appropriate size of the backlog is 1024, though you may
   * play around with this value during performance testing.
   */

  /* PART 1 BEGIN */
  bind(*socket_number,(struct sockaddr*)&server_address,sizeof(server_address));
  listen(*socket_number,1024);

  /* PART 1 END */
  printf("Listening on port %d...\n", server_port);

#ifdef POOLSERVER
  /*
   * The thread pool is initialized *before* the server
   * begins accepting client connections.
   */
  init_thread_pool(num_threads, request_handler);
#endif

  while (1) {
    client_socket_number = accept(*socket_number, (struct sockaddr*)&client_address,
                                  (socklen_t*)&client_address_length);
    if (client_socket_number < 0) {
      perror("Error accepting socket");
      continue;
    }

    printf("Accepted connection from %s on port %d\n", inet_ntoa(client_address.sin_addr),
           client_address.sin_port);

#ifdef BASICSERVER
    /*
     * This is a single-process, single-threaded HTTP server.
     * When a client connection has been accepted, the main
     * process sends a response to the client. During this
     * time, the server does not listen and accept connections.
     * Only after a response has been sent to the client can
     * the server accept a new connection.
     */
    request_handler(client_socket_number);

#elif FORKSERVER
    /*
     * TODO: PART 5
     *
     * When a client connection has been accepted, a new
     * process is spawned. This child process will send
     * a response to the client. Afterwards, the child
     * process should exit. During this time, the parent
     * process should continue listening and accepting
     * connections.
     */

    /* PART 5 BEGIN */
    pid_t pid = fork();
    if(pid < 0){  
      perror("Failed to fork");
      close(client_socket_number);
    }else if(pid == 0){ // child process
      close(*socket_number); // child process doesn't need the server socket
      request_handler(client_socket_number);
      exit(0);
    }else{
      close(client_socket_number);
    }
    /* PART 5 END */

#elif THREADSERVER
    /*
     * TODO: PART 6
     *
     * When a client connection has been accepted, a new
     * thread is created. This thread will send a response
     * to the client. The main thread should continue
     * listening and accepting connections. The main
     * thread will NOT be joining with the new thread.
     */

    /* PART 6 BEGIN */

    /* PART 6 END */
#elif POOLSERVER
    /*
     * TODO: PART 7
     *
     * When a client connection has been accepted, add the
     * client's socket number to the work queue. A thread
     * in the thread pool will send a response to the client.
     */

    /* PART 7 BEGIN */

    /* PART 7 END */
#endif
  }

  shutdown(*socket_number, SHUT_RDWR);
  close(*socket_number);
}

int server_fd;
void signal_callback_handler(int signum) {
  printf("Caught signal %d: %s\n", signum, strsignal(signum));
  printf("Closing socket %d\n", server_fd);
  if (close(server_fd) < 0)
    perror("Failed to close server_fd (ignoring)\n");
  exit(0);
}

char* USAGE =
    "Usage: ./httpserver --files some_directory/ [--port 8000 --num-threads 5]\n"
    "       ./httpserver --proxy inst.eecs.berkeley.edu:80 [--port 8000 --num-threads 5]\n";

void exit_with_usage() {
  fprintf(stderr, "%s", USAGE);
  exit(EXIT_SUCCESS);
}

int main(int argc, char** argv) {
  signal(SIGINT, signal_callback_handler);
  signal(SIGPIPE, SIG_IGN);

  /* Default settings */
  server_port = 8000;
  void (*request_handler)(int) = NULL;

  int i;
  for (i = 1; i < argc; i++) {
    if (strcmp("--files", argv[i]) == 0) {
      request_handler = handle_files_request;
      server_files_directory = argv[++i];
      if (!server_files_directory) {
        fprintf(stderr, "Expected argument after --files\n");
        exit_with_usage();
      }
    } else if (strcmp("--proxy", argv[i]) == 0) {
      request_handler = handle_proxy_request;

      char* proxy_target = argv[++i];
      if (!proxy_target) {
        fprintf(stderr, "Expected argument after --proxy\n");
        exit_with_usage();
      }

      char* colon_pointer = strchr(proxy_target, ':');
      if (colon_pointer != NULL) {
        *colon_pointer = '\0';
        server_proxy_hostname = proxy_target;
        server_proxy_port = atoi(colon_pointer + 1);
      } else {
        server_proxy_hostname = proxy_target;
        server_proxy_port = 80;
      }
    } else if (strcmp("--port", argv[i]) == 0) {
      char* server_port_string = argv[++i];
      if (!server_port_string) {
        fprintf(stderr, "Expected argument after --port\n");
        exit_with_usage();
      }
      server_port = atoi(server_port_string);
    } else if (strcmp("--num-threads", argv[i]) == 0) {
      char* num_threads_str = argv[++i];
      if (!num_threads_str || (num_threads = atoi(num_threads_str)) < 1) {
        fprintf(stderr, "Expected positive integer after --num-threads\n");
        exit_with_usage();
      }
    } else if (strcmp("--help", argv[i]) == 0) {
      exit_with_usage();
    } else {
      fprintf(stderr, "Unrecognized option: %s\n", argv[i]);
      exit_with_usage();
    }
  }

  if (server_files_directory == NULL && server_proxy_hostname == NULL) {
    fprintf(stderr, "Please specify either \"--files [DIRECTORY]\" or \n"
                    "                      \"--proxy [HOSTNAME:PORT]\"\n");
    exit_with_usage();
  }

#ifdef POOLSERVER
  if (num_threads < 1) {
    fprintf(stderr, "Please specify \"--num-threads [N]\"\n");
    exit_with_usage();
  }
#endif

  chdir(server_files_directory);
  serve_forever(&server_fd, request_handler);

  return EXIT_SUCCESS;
}
