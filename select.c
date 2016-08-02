/* $begin tinymain */
/*
 *  * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the 
 *   *     GET method to serve static and dynamic content.
 *    */
#include "csapp.h"

void doit(int fd);
void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);

void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg);

void sigchld_handler(int sig) //line:conc:echoserverp:handlerstart
{
    while (waitpid(-1, 0, WNOHANG) > 0)
	;
    return;
} //line:conc:echoserverp:handlerend
int byte_cnt = 0;

int main(int argc, char **argv) 
{
    int listenfd, connfd, port, clientlen;
/*
 * struct sockaddr_in {
 *		unsigned short sin_family;
 *		unsigned short sin_port;
 *		struct   in_addr sin_addr;
 *		unsigned char sin_zero[8];
 *
 * };
 */
    struct sockaddr_in clientaddr;
	static pool pool;

    /* Check command line args */
	/* add root index */
    if (argc != 3) {
		fprintf(stderr, "usage: %s <port> <root directory>\n", argv[0]);
		exit(1);
    }
	if ( chdir( argv[2] ) < 0 ) {
		fprintf(stderr, "error: %s root directory no permissions\n", argv[2]);
		exit(1);
	}
    port = atoi(argv[1]);

	signal(SIGPIPE, SIG_IGN); //SIG_IGN is the ignore signal handler
	//signal(SIGCHLD, sigchld_handler);
    listenfd = Open_listenfd(port);
	clientlen = sizeof(clientaddr);
	init_pool(listenfd, &pool);
    while (1) {
		pool.ready_set = pool.read_set;
		pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);
		if ( FD_ISSET(listenfd, &pool.ready_set) ) {
			connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
			add_client(connfd, &pool);
		}
		check_clients(&pool);
    }
}
/* $end tinymain */

/*
 *  * doit - handle one HTTP request/response transaction
 *   */
/* $begin doit */
void doit(int fd) 
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;
  
    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);                   //line:netp:doit:readrequest
    sscanf(buf, "%s %s %s", method, uri, version);       //line:netp:doit:parserequest
	printf("uri=%s\n", uri);
    if (strcasecmp(method, "GET")) {                     //line:netp:doit:beginrequesterr
       clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
       return;
    }                                                    //line:netp:doit:endrequesterr
    read_requesthdrs(&rio);                              //line:netp:doit:readrequesthdrs

    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs);       //line:netp:doit:staticcheck
    if (stat(filename, &sbuf) < 0) {                     //line:netp:doit:beginnotfound
		clienterror(fd, filename, "404", "Not found",
			    "Tiny couldn't find this file");
		return;
    }                                                    //line:netp:doit:endnotfound

    if (is_static) { /* Serve static content */          
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //line:netp:doit:readable
		    clienterror(fd, filename, "403", "Forbidden",
				"Tiny couldn't read the file");
		    return;
		}
		serve_static(fd, filename, sbuf.st_size);        //line:netp:doit:servestatic
    } else { /* Serve dynamic content */
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //line:netp:doit:executable
		    clienterror(fd, filename, "403", "Forbidden",
				"Tiny couldn't run the CGI program");
		    return;
		}
		serve_dynamic(fd, filename, cgiargs);            //line:netp:doit:servedynamic
    }
}
/* $end doit */

/*
 *  * read_requesthdrs - read and parse HTTP request headers
 *   */
/* $begin read_requesthdrs */
void read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];
	int d;

    Rio_readlineb(rp, buf, MAXLINE);
    while( d = strcmp(buf, "\r\n") ) {          //line:netp:readhdrs:checkterm
		Rio_readlineb(rp, buf, MAXLINE);
		printf("%s", buf);
    }
    return;
}
/* $end read_requesthdrs */

/*
 *  * parse_uri - parse URI into filename and CGI args
 *   *             return 0 if dynamic content, 1 if static
 *    */
/* $begin parse_uri */
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */ //line:netp:parseuri:isstatic
		strcpy(cgiargs, "");                             //line:netp:parseuri:clearcgi
		strcpy(filename, ".");                           //line:netp:parseuri:beginconvert1
		strcat(filename, uri);                           //line:netp:parseuri:endconvert1
		if (uri[strlen(uri)-1] == '/')                   //line:netp:parseuri:slashcheck
		    strcat(filename, "index.html");               //line:netp:parseuri:appenddefault
		return 1;
    }
    else {  /* Dynamic content */                        //line:netp:parseuri:isdynamic
		ptr = index(uri, '?');                           //line:netp:parseuri:beginextract
		if (ptr) {
		    strcpy(cgiargs, ptr+1);
		    *ptr = '\0'; //delete ?....
		}
		else 
		    strcpy(cgiargs, "");                         //line:netp:parseuri:endextract
		strcpy(filename, ".");                           //line:netp:parseuri:beginconvert2
		strcat(filename, uri);                           //line:netp:parseuri:endconvert2
		return 0;
    }
}
/* $end parse_uri */

/*
 *  * serve_static - copy a file back to the client 
 *   */
/* $begin serve_static */
void serve_static(int fd, char *filename, int filesize) 
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];
 
    /* Send response headers to client */
    get_filetype(filename, filetype);       //line:netp:servestatic:getfiletype
    sprintf(buf, "HTTP/1.0 200 OK\r\n");    //line:netp:servestatic:beginserve
    sprintf(buf, "%sServer: Tiny Server\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
    Rio_writen(fd, buf, strlen(buf));       //line:netp:servestatic:endserve

    /* Send response body to client */
    srcfd = Open(filename, O_RDONLY, 0);    //line:netp:servestatic:open
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);//line:netp:servestatic:mmap
    Close(srcfd);                           //line:netp:servestatic:close
    Rio_writen(fd, srcp, filesize);         //line:netp:servestatic:write
    Munmap(srcp, filesize);                 //line:netp:servestatic:munmap
}

/*
 *  * get_filetype - derive file type from file name
 *   */
void get_filetype(char *filename, char *filetype) 
{
    if (strstr(filename, ".html"))
		strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
		strcpy(filetype, "image/gif");
    else if (strstr(filename, ".jpg"))
		strcpy(filetype, "image/jpeg");
    else
		strcpy(filetype, "text/plain");
}  
/* $end serve_static */

/*
 *  * serve_dynamic - run a CGI program on behalf of the client
 *   */
/* $begin serve_dynamic */
void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    if (Fork() == 0) { /* child */ //line:netp:servedynamic:fork
		/* Real server would set all CGI vars here */
		setenv("QUERY_STRING", cgiargs, 1); //line:netp:servedynamic:setenv
		Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //line:netp:servedynamic:dup2
		Execve(filename, emptylist, environ); /* Run CGI program */ //line:netp:servedynamic:execve
    }
    Wait(NULL); /* Parent waits for and reaps child */ //line:netp:servedynamic:wait
}
/* $end serve_dynamic */

/*
 *  * clienterror - returns an error message to the client
 *   */
/* $begin clienterror */
void clienterror(int fd, char *cause, char *errnum, 
		 char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE], body[MAXBUF];

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

    /* Print the HTTP response */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}
/* $end clienterror */

void init_pool(int listenfd, pool *p)
{
	int i;
	p->maxi = -1;
	for (i=0; i< FD_SETSIZE; i++)
		p->clientfd[i] = -1;
	p->maxfd = listenfd;
	FD_ZERO(&p->read_set);
	FD_SET(listenfd, &p->read_set);
}

void add_client(int connfd, pool *p)
{
	int i;
	p->nready--;
    for (i = 0; i < FD_SETSIZE; i++)  /* Find an available slot */
    	if (p->clientfd[i] < 0) {
    	    /* Add connected descriptor to the pool */
    	    p->clientfd[i] = connfd;                 //line:conc:echoservers:beginaddclient
    	    Rio_readinitb(&p->clientrio[i], connfd); //line:conc:echoservers:endaddclient

    	    /* Add the descriptor to descriptor set */
    	    FD_SET(connfd, &p->read_set); //line:conc:echoservers:addconnfd

    	    /* Update max descriptor and pool highwater mark */
    	    if (connfd > p->maxfd) //line:conc:echoservers:beginmaxfd
    	    p->maxfd = connfd; //line:conc:echoservers:endmaxfd
    	    if (i > p->maxi)       //line:conc:echoservers:beginmaxi
    	    p->maxi = i;       //line:conc:echoservers:endmaxi
    	    break;
    	}
    if (i == FD_SETSIZE) /* Couldn't find an empty slot */
    	app_error("add_client error: Too many clients");
}

void check_clients(pool *p)
{
    int i, connfd, n, is_static;
    rio_t rio;
	struct stat sbuf;
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];

    for (i = 0; (i <= p->maxi) && (p->nready > 0); i++) {
        connfd = p->clientfd[i];
        rio = p->clientrio[i];

        /* fd is ready */
        if ((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))) {
			Rio_readinitb(&rio, connfd);
			Rio_readlineb(&rio, buf, MAXLINE);
			sscanf(buf, "%s %s %s", method, uri, version); 
			read_requesthdrs(&rio);
			is_static = parse_uri(uri, filename, cgiargs);
			stat(filename, &sbuf);
			serve_static(connfd, filename, sbuf.st_size);

            Close(connfd);
            FD_CLR(connfd, &p->read_set);
            p->clientfd[i] = -1;
        }
    }
}
