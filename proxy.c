/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS: (put your names here)
 *     Student Name1, student1@cs.uky.edu 
 *     Student Name2, student2@cs.uky.edu 
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include "csapp.h"

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void echo(int connfd);
void readStuff(int connfd, char * buf, int endCharLength);
void writeStuff(int connfd, char * buf);
#define ENDREQUEST 2
#define ENDRESPONSE 0
/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{
	int listenfd, connfd, proxyPort, clientlen;
	int serverfd, serverPort;
	struct sockaddr_in clientaddr;
	struct hostent *hp;
	char * haddrp;
	char uri[MAXLINE];
	char hostname[MAXLINE];
	char pathname[MAXLINE];
	char method[MAXLINE];
	struct in_addr * inAddr;
	char reqBuf[MAXLINE];
	char respBuf[MAXLINE];	
    /* Check arguments */
    if (argc != 2) 
	{
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
    }
	proxyPort = atoi(argv[1]);

	/* Open a socket */
	listenfd = Open_listenfd(proxyPort);

	/* Listen for request */
	while(1)
	{
		clientlen = sizeof(clientaddr);
		/* Accept the incoming connection on listenfd */
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
		/* Read request into reqBuf */ 
		readStuff(connfd, reqBuf, ENDREQUEST);
		/* Parse reqBuf to get uri */
		printf("reqBuf: %s", reqBuf);
		sscanf(reqBuf, "%s %s", method, uri);
		printf("URI: %s\n", uri);
		/* Parse uri into host, path and port */
		parse_uri(uri, hostname, pathname, &serverPort);
		if(strlen(hostname) <= 0)
			continue;
		/* Connect to socket on server denoted by hostname */
		serverfd = Open_clientfd(hostname, serverPort);
		printf("HOST: %s\t PATH: %s\t PORT: %d\n", hostname, pathname, serverPort);
		writeStuff(serverfd, reqBuf);
		readStuff(serverfd, respBuf, ENDRESPONSE);
		printf("RESPBUF: %s\n", respBuf);
		Close(serverfd);
		/* Check for disallowed words */
		writeStuff(connfd, respBuf);
		Close(connfd);
	}
    exit(0);
}


/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
	hostname[0] = '\0';
	return -1;
    }
       
    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
	*port = atoi(hostend + 1);
    
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
	pathname[0] = '\0';
    }
    else {
	pathbegin++;	
	strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s", time_str, a, b, c, d, uri);
}

/**
 * echo - output the contents of a file.
 *
 * connfd - socket file descriptor. 
 * 
 */
void echo(int connfd)
{
	size_t n;
	char buf[MAXLINE];
	rio_t rio;

	Rio_readinitb(&rio, connfd);
	while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
	{
		printf("Proxy received %d bytes\n", n);
		printf("BUF: %s\n", buf);
		Rio_writen(connfd, buf, n);
	}
}

void readStuff(int connfd, char * buf, int endCharLength)
{
	size_t n;
	int totalBytes = 0;
	rio_t rio;
	Rio_readinitb(&rio, connfd);
	while((n = Rio_readlineb(&rio, &buf[totalBytes], MAXLINE)) > endCharLength)
	{
		totalBytes += n;
		if(totalBytes > MAXLINE)
		{
			printf("You have overrun your buffer.");
			exit(-1);
		}
		//printf("BUF: %s", buf);
	}
}

void writeStuff(int connfd, char * buf)
{
	//printf("STRLEN: %d", strlen(buf));
	Rio_writen(connfd, buf, strlen(buf));
}
