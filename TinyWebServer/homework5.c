#include <fnmatch.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <sys/wait.h>

#define BACKLOG (10)
void sendHelper(int client_fd, int read_fd, char * request_string, char * send_buf) ;
void serve_request(int);
void getFileType(char *fileName, char *fileType);

char * request_str = "HTTP/1.0 200 OK\r\n"
        "Content-type: text/html; charset=UTF-8\r\n\r\n";





char * index_hdr = "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\"><html>"
        "<title>Directory listing for %s</title>"
"<body>"
"<h2>Directory listing for %s</h2><hr><ul>";

// snprintf(output_buffer,4096,index_hdr,filename,filename);


char * index_body = "<li><a href=\"%s\">%s</a>";

char * index_ftr = "</ul><hr></body></html>";

/* char* parseRequest(char* request)
 * Args: HTTP request of the form "GET /path/to/resource HTTP/1.X"
 *
 * Return: the resource requested "/path/to/resource"
 *         0 if the request is not a valid HTTP request
 *
 * Does not modify the given request string.
 * The returned resource should be free'd by the caller function.
 */
char* parseRequest(char* request){
  //assume file paths are no more than 256 bytes + 1 for null.
  char *buffer = malloc(sizeof(char)*257);
  if(buffer == NULL){
    perror("malloc fail");
    exit(1);
  }
  memset(buffer, 0, 257);

  if(fnmatch("GET * HTTP/1.*",  request, 0)) return 0;

  sscanf(request, "GET %s HTTP/1.", buffer);
  return buffer;
}

//Function to run a command using the user's args, and return the output.
void isCGI(int client_fd, char * requested_file){
  char *request_string = "HTTP/1.0 200 OK\r\n"
      "Content-type: text/plain; charset=UTF-8\r\n\r\n";

  char func[4096];
  char str1[4096];
  char str2[4096];

  sscanf(requested_file,"/%[^?]?%[^&]&%s", func, str1, str2);


  char * function = "./format_string";
  char* args[3];
  args[0] = function; //"./format_string";
  args[1] = str1;
  args[2] = str2;

  pid_t pid;
  int status;


  int fd[2];
  pipe(fd);



  if((pid = fork()) == 0){
    if(close(fd[0]) < 0){
      perror("close failed11");
      //exit(1);
    }
    //fflush(stdout);
    dup2(fd[1], 1);
    if(close(fd[1]) < 0){
      perror("close failed11");
      //exit(1);
    }
    if(execvp(args[0], args) <  0){
      perror("command not found");
      //exit(1);
    }
  }


  if(waitpid(pid, &status, 0|WUNTRACED) < 0){
   perror("waiting for child error");
 }
  char buffer[4096];

  if (close(fd[1]) < 0){
    perror("close failed2");
    exit(1);
  }
  if (read(fd[0], buffer, sizeof(buffer)) < 0){
    perror("read fail");
    exit(1);
   }


  if(send(client_fd,request_string,strlen(request_string),0) < 0){
    if (errno == EPIPE){
      if(close(client_fd) < 0){
        perror("close failed");
        exit(1);
      }
      perror("broken pipe");
      pthread_exit(NULL);
    }
    perror("send fail");
    exit(1);
  }


  int sent = 0;
  int _sent = 0;
  while (sent < strlen(buffer)){
    _sent = send(client_fd, buffer + sent, strlen(buffer) - sent, 0);
      if(_sent < 0){
        if (errno == EPIPE){
          if(close(client_fd) < 0){
              perror("close failed11");
              exit(1);
          }
          perror("broken pipe");
          pthread_exit(NULL);
        }
        perror("send fail");
        exit(1);
      }
    sent = sent + _sent;
  }
}


//Function to help with sending files to client.
void sendHelper(int client_fd, int read_fd, char * request_string, char * send_buf) {
  int bytes_read;
  if(send(client_fd,request_string,strlen(request_string),0) < 0){
    if (errno == EPIPE){
      if(close(client_fd) < 0){
        perror("close failed11");
        //exit(1);
      }
      perror("broken pipe");
      pthread_exit(NULL);
    }
  perror("send fail");
  //exit(1);
  }

  while(1){
    bytes_read = read(read_fd,send_buf,4096);
    if(bytes_read < 0){
      perror("read fail");
      //exit(1);
    }

    if(bytes_read == 0){
      break;
    }

    if(send(client_fd,send_buf,bytes_read,0) < 0){
      if (errno == EPIPE){
        if(close(client_fd) < 0){
          perror("close failed11");
          //exit(1);
        }
        perror("broken pipe");
        pthread_exit(NULL);
      }
      perror("send fail");
      //exit(1);
    }
  }
}


//Function to send a 404 error if the request is not found.
void notFound(int client_fd){

  char *request_string = "HTTP/1.0 404 Not found\r\n"
      "Content-type: text/html; charset=UTF-8\r\n\r\n"
      "<html><head><title>404 Not found</title></head>"
      "<body>"
      "<strong>"
      "<font size=\"20\">"

       "<pre>"
       " _  _    ___  _  _     _   _  ____ _______   ______ ____  _    _ _   _ _____ <br>"
       "| || |  / _ \\| || |   | \\ | |/ __ \\__   __| |  ____/ __ \\| |  | | \\ | |  __ \\<br>"
       "| || |_| | | | || |_  |  \\| | |  | | | |    | |__ | |  | | |  | |  \\| | |  | |<br>"
       "|__   _| | | |__   _| | . ` | |  | | | |    |  __|| |  | | |  | | . ` | |  | |<br>"
       "   | | | |_| |  | |   | |\\  | |__| | | |    | |   | |__| | |__| | |\\  | |__| |<br>"
       "   |_|  \\___/   |_|   |_| \\_|\\____/  |_|    |_|    \\____/ \\____/|_| \\_|_____/<br>"


       "</pre>"





      "</font></strong>"
      "</body></html>";



  send(client_fd,  request_string, strlen(request_string), 0);

}


//Function to send the index.html file of a directory.
void sendIndex(int client_fd, char *path){
  int read_fd = open(path,0,0);
  if(read_fd < 0){
    perror("open fail");
    //exit(1);
  }
  char * request_string = "HTTP/1.0 200 OK\r\nContent-type: text/html; charset=UTF-8\r\n\r\n";
  char send_buf[4096];
  sendHelper(client_fd, read_fd, request_string, send_buf);
}


//Function to dynamically create a directory list in HTML.
void sendList(int client_fd, char *filename, char *path){

  char send_buf[4096];
  char *request_string = "HTTP/1.0 200 OK\r\n"
                         "Content-type: text/html; charset=UTF-8\r\n\r\n";
  if(send(client_fd,request_string,strlen(request_string),0) < 0){
    if (errno == EPIPE){
      if(close(client_fd) < 0){
        perror("close failed11");
        //exit(1);
      }
      perror("broken pipe");
      pthread_exit(NULL);
    }
    perror("send fail");
    //exit(1);
  }


  strcpy(send_buf,"<html>\n<head>\n<title>\nLab 4 test pages\n</title>\n</head>\n<body>"
                  "<strong>"
                  "<font size=\"20\">Directory<br></font><hr>"
                  "</strong>");


  if(send(client_fd,send_buf,strlen(send_buf),0) < 0){
    if (errno == EPIPE){
      if(close(client_fd) < 0){
        perror("close failed11");
        exit(1);
      }
      perror("broken pipe");
      pthread_exit(NULL);
    }
    perror("send fail");
    exit(1);
  }

  struct dirent *dir;
  DIR * d = opendir(filename);
  if (d == NULL){
    perror("dir failed to open");
    exit(1);
  }

  if(d){
    while ((dir = readdir(d)) != NULL){
      if((strcmp(dir->d_name, "..") == 0) || (strcmp(dir->d_name, ".") == 0)){
        continue;
      }
      char link[1012]  = "";//"./";

      //strcat(link, dir->d_name,);
      memcpy(&link[0], dir->d_name, strlen(dir->d_name) );/////////////////////////////

      if(dir->d_type  == DT_DIR){
        strcat(link, "/");
      }

      sprintf(send_buf, "<a href=\"%s\">%s<br></a>\n", link, dir->d_name);
      int sent = 0;
      int _sent = 0;
      while (sent < strlen(send_buf)){
        _sent = send(client_fd, send_buf + sent, strlen(send_buf) - sent, 0);
        if(_sent < 0){
          if (errno == EPIPE){
            if(close(client_fd) < 0){
              perror("close failed11");
              exit(1);
            }
            perror("broken pipe");
            pthread_exit(NULL);
          }
          perror("send fail");
          exit(1);
        }
        sent = sent + _sent;
      }
    }
    if(errno == EBADF){
      perror("Opening dir failed");
      exit(1);
    }
    if(closedir(d) < 0){
      perror("close failed1");
      exit(1);
    }
    strcpy(send_buf,
                "</body>\n"
                "</html>");
    int sent = 0;
    int _sent = 0;
    while (sent < strlen(send_buf)){
      _sent = send(client_fd, send_buf + sent, strlen(send_buf) - sent, 0);
      if(_sent < 0){
        if (errno == EPIPE){
          if(close(client_fd) < 0){
            perror("close failed11");
            exit(1);
          }
          perror("broken pipe");
          pthread_exit(NULL);
        }
        perror("send fail");
        exit(1);
      }
      sent = sent + _sent;
    }
  }
}

//Function to send files (.pdf, .png, .jpg, text, plain, etc.)
void sendFile(int client_fd, int read_fd){//, char *request_string){
  int bytes_read;
  char send_buf[4096];

  if(send(client_fd,request_str,strlen(request_str),0) < 0){

    if (errno == EPIPE){
      if(close(client_fd) < 0){
        perror("close failed11");
        //exit(1);
      }
      perror("broken pipe");
      pthread_exit(NULL);
    }
    perror("send fail");
    //exit(1);
  }

  while(1){
    bytes_read = read(read_fd,send_buf,4096);
    if(bytes_read < 0){
      perror("read failed");
      //exit(1);
    }
    if(bytes_read == 0){
      break;
    }
    if(send(client_fd,send_buf,bytes_read,0) < 0){
      if (errno == EPIPE){
        if(close(client_fd) < 0){
          perror("close failed11");
          //exit(1);
        }
        perror("broken pipe");
        pthread_exit(NULL);
      }
      perror("send fail");
      //exit(1);
    }
  }
}

//Function to read request, then send the requested file.
void serve_request(int client_fd){
  int read_fd;
  int file_offset = 0;
  char client_buf[4096];
  char filename[4096];
  char *requested_file;
  char fileType[4096];
  char newRequestString[8128];
  memset(client_buf,0,4096);
  memset(filename,0,4096);
  struct stat file_stat;
  int recvReturn = 0;

  while(1){
    recvReturn = recv(client_fd,&client_buf[file_offset],4096,0);

    if(recvReturn < 0){
      perror("recv fail");
      //exit(1);//////////////
    }

    file_offset = recvReturn + file_offset;

    if(strstr(client_buf,"\r\n\r\n")){
      break;
    }
  }

  requested_file = parseRequest(client_buf);

  if(requested_file == NULL){
    return;
  }
  if(strstr(requested_file, "format")){
    isCGI(client_fd, requested_file);
    free(requested_file);
    return;
  }

  getFileType(requested_file, fileType);
  sprintf(newRequestString, "HTTP/1.0 200 OK\r\n"
         "Content-type: %s charset=UTF-8\r\n\r\n", fileType);
  request_str = newRequestString;
  filename[0] = '.';


  //memcpy(&filename[1], requested_file, 4095);////////////////////////////////////////////////
  strncpy(&filename[1],requested_file,4095);

  //Checking if file is found. If not found then 404 page is returned.
  if ((stat(filename, &file_stat) != 0)){
    notFound(client_fd);
    free(requested_file);
    return;
  }
  else{
    read_fd = open(filename,0,0);
    if(read_fd < 0){
      perror("file failed to open");
      //exit(1);
    }

    if(S_ISDIR(file_stat.st_mode)){
      char path[4096] = "";
      strcat(path ,filename);
      //memcpy(path, filename, strlen(filename) );
      // memcpy(path, )
      strcat(path, "index.html");

      if(stat(path, &file_stat) == 0){///////////////////////////////////////
        sendIndex(client_fd, path);

        // if(close(client_fd) < 0){
        //   perror("close failed3");
        //   exit(1);
        // }
        free(requested_file);
        return;
      }
      else{

        sendList(client_fd, filename, filename);

        if(close(read_fd) < 0){
          perror("close failed3");
          //exit(1);
        }
        // if(close(client_fd)<0){
        //  perror("close failed3");
        //  exit(1);
        // }
        free(requested_file);
        return;

      }
    }
  }

  sendFile(client_fd, read_fd);//, request_str);

  if( close(read_fd) < 0){
    perror("close failed4");
    //exit(1);
  }
  // if(close(client_fd) < 0){
  //   perror("close failed5");
  //   exit(1);
  // }
  free(requested_file);
  return;
}


//server_request wrapper for pthread_create function
void * _serve_request (void * args){
	serve_request(*((int*) args));
  if(pthread_detach(pthread_self()) != 0){
       //exit(1);
  }
  if(close(*((int*) args)) < 0){
    //perror("close failed6");
    //exit(1);
  }


  return NULL;
}

//Function to get the file type of a request.
void getFileType(char *fileName, char *fileType){

	if(strstr(fileName, ".html")){
		strcpy(fileType, "text/html");
	}
	else if(strstr(fileName, ".gif")){
		strcpy(fileType, "image/gif");
	}
	else if(strstr(fileName, ".png")){
		strcpy(fileType, "image/png");
	}
	else if(strstr(fileName, ".jpg")){
		strcpy(fileType, "image/jpeg");
	}
	else if(strstr(fileName, ".pdf")){
		strcpy(fileType, "application/pdf"); // <-- may be wrong for PDF
	}
  else if(strstr(fileName, ".ico")){
    strcpy(fileType, "image/icon");
  }
	else{
		strcpy(fileType, "text/plain");
	}
}


/* Your program should take two arguments:
 * 1) The port number on which to bind and listen for connections, and
 * 2) The directory out of which to serve files.
 */
int main(int argc, char** argv) {
  signal(SIGPIPE, SIG_IGN);
  /* For checking return values. */
  int retval;


  if(argc != 3){
		perror("Missing arguments");
		//exit(1);
	 }


  int port = atoi(argv[1]);


  if (chdir(argv[2]) != 0){
    perror("chdir() failed");
    //exit(1);
  }


	   /* Create a socket to which clients will connect. */
  int server_sock = socket(AF_INET6, SOCK_STREAM, 0);
  if(server_sock < 0) {
    perror("Creating socket failed");
    exit(1);
  }

  /* A server socket is bound to a port, which it will listen on for incoming
   * connections.  By default, when a bound socket is closed, the OS waits a
   * couple of minutes before allowing the port to be re-used.  This is
   * inconvenient when you're developing an application, since it means that
   * you have to wait a minute or two after you run to try things again, so
   * we can disable the wait time by setting a socket option called
   * SO_REUSEADDR, which tells the OS that we want to be able to immediately
   * re-bind to that same port. See the socket(7) man page ("man 7 socket")
   * and setsockopt(2) pages for more details about socket options. */
  int reuse_true = 1;
  retval = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true, sizeof(reuse_true));
  if (retval < 0) {
      perror("Setting socket option failed");
      exit(1);
  }

  /* Create an address structure.  This is very similar to what we saw on the
   * client side, only this time, we're not telling the OS where to connect,
   * we're telling it to bind to a particular address and port to receive
   * incoming connections.  Like the client side, we must use htons() to put
   * the port number in network byte order.  When specifying the IP address,
   * we use a special constant, INADDR_ANY, which tells the OS to bind to all
   * of the system's addresses.  If your machine has multiple network
   * interfaces, and you only wanted to accept connections from one of them,
   * you could supply the address of the interface you wanted to use here. */


  struct sockaddr_in6 addr;   // internet socket address data structure
  addr.sin6_family = AF_INET6;
  addr.sin6_port = htons(port); // byte order is significant
  addr.sin6_addr = in6addr_any; // listen to all interfaces


  /* As its name implies, this system call asks the OS to bind the socket to
   * address and port specified above. */
  retval = bind(server_sock, (struct sockaddr*)&addr, sizeof(addr));
  if(retval < 0) {
      perror("Error binding to port");
      exit(1);
  }

  /* Now that we've bound to an address and port, we tell the OS that we're
   * ready to start listening for client connections.  This effectively
   * activates the server socket.  BACKLOG (#defined above) tells the OS how
   * much space to reserve for incoming connections that have not yet been
   * accepted. */
  retval = listen(server_sock, BACKLOG);
  if(retval < 0) {
      perror("Error listening for connections");
      exit(1);
  }

  int *sockets = malloc(1000 * sizeof(int));
  if (sockets == NULL) {
      perror("malloc() failed");
      exit(1);
  }
  while(1) {
      /* Declare a socket for the client connection. */
      int sock;
      //char buffer[256];

      /* Another address structure.  This time, the system will automatically
       * fill it in, when we accept a connection, to tell us where the
       * connection came from. */
      struct sockaddr_in remote_addr;
      unsigned int socklen = sizeof(remote_addr);

      /* Accept the first waiting connection from the server socket and
       * populate the address information.  The result (sock) is a socket
       * descriptor for the conversation with the newly connected client.  If
       * there are no pending connections in the back log, this function will
       * block indefinitely while waiting for a client connection to be made.
       * */
      sock = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen);

      if(sock < 0) {
          perror("Error accepting connection");
          //exit(1);
      }
      sockets[sock] = sock;
      /* At this point, you have a connected socket (named sock) that you can
       * use to send() and recv(). */

      /* ALWAYS check the return value of send().  Also, don't hardcode
       * values.  This is just an example.  Do as I say, not as I do, etc. */

      //serve_request(sock);

      pthread_t clientThreadID;
      int threadStatus = pthread_create(&clientThreadID, NULL, _serve_request, &sockets[sock]);
	    if(threadStatus != 0){
		   perror("Error creating thread");
		   //exit(1);
	    }
      // if(pthread_detach(clientThreadID) != 0){
      //  perror("Error detaching thread");
      //  exit(1);
      // }
       //pthread_join(clientThreadID, NULL);




  //pthread_join(clientThreadID, NULL);

	//printf("<- thread!\n");

      /* Tell the OS to clean up the resources associated with that client
       * connection, now that we're done with it. */
  //close(sock);
  }

  if(close(server_sock) < 0){
    perror("close failed7");
    //exit(1);
  }
}
