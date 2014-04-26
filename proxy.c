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
void format_log_entry(FILE * proxyLog, struct sockaddr_in *sockaddr, char *uri, int size, int blocked);
void echo(int connfd);
int readRequest(int connfd, char * buf, int contentLength);
void writeStuff(int connfd, char * buf, int contentLength);

typedef struct respBuf
{
	char * respContent;
	int respCeiling;

} RespBuf;

int readResponse(int connfd, RespBuf * respBuf);
#define REQUEST 1
#define RESPONSE 0
#define MAXNET 65536
/* 
 * main - Main routine for the proxy program 
 */
static char * disallowedPage = "HTTP/1.1 200 OK\n"
								"Content-Length: 83\n"
								"Content-Type: text/html\n"
								"\n"
								"<!DOCTYPE html>"
									"<head></head>"
									"<body>"
										"<h1>This page has disallowed content.</h1>"
									"</body>"
								"</html>";
int main(int argc, char **argv)
{
	int listenfd, connfd, proxyPort, clientlen;
	int serverfd, serverPort, reqLen, respLen, contLen;
	struct sockaddr_in clientaddr;
	char uri[MAXLINE];
	char hostname[MAXLINE];
	char pathname[MAXLINE];
	char method[MAXLINE];
	char reqBuf[MAXNET];
	RespBuf * respBuf;
	char word[MAXLINE];
	char * contentLength;
	char ** disallowedWords;
	FILE * disAllowedWordsFile;
  	FILE * proxyLog;	
   	/* Check arguments */
	if (argc != 2) 
	{
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
	}
	/* Initialize disAllowedWords from file */
	if((disAllowedWordsFile = fopen("DisallowedWords", "r")) == NULL)
	{
		perror("COULD NOT OPEN DisallowedWords");
		exit(-1);
	}
	int i = 0;
	char * newLine;
	while(fgets(word, MAXLINE, disAllowedWordsFile) != NULL)
	{
		/* Count number of lines in file */
		i++;
	}
	disallowedWords = malloc(sizeof(char *) * i);
	if(disallowedWords == NULL)
		perror("Out of memory.");
	i = 0;
	if(fseek(disAllowedWordsFile, 0, SEEK_SET) != 0)
		perror("FSEEK");
	while(fgets(word, MAXLINE, disAllowedWordsFile) != NULL)
	{
		/* Remove newlines from disAllowedWords */
		newLine = strchr(word, '\n');
		*newLine = '\0';
		disallowedWords[i] = malloc(sizeof(word));
		strncpy(disallowedWords[i], word, sizeof(word));
		i++;
	}
	/* Set NULL sentinel at the end of disAllowedWords array */
	disallowedWords[i] = NULL;
	fclose(disAllowedWordsFile);
	/* Open proxy.log */
	if((proxyLog = fopen("proxy.log", "w+")) == NULL)
	{
		perror("COULD NOT OPEN proxy.log");
		exit(-1);
	}
	proxyPort = atoi(argv[1]);

	/* Open a socket */
	listenfd = Open_listenfd(proxyPort);

	/* Initialize respBuf struct */
	if((respBuf = malloc(sizeof(RespBuf))) == NULL)
	{
		perror("Out of memory.");
		exit(-1);
	}
	respBuf->respCeiling = MAXNET;
	if((respBuf->respContent = malloc(MAXNET)) == NULL)
	{
		perror("Out of memory.");
		exit(-1);
	}

	/* Listen for request */
	while(1)
	{
		clientlen = sizeof(clientaddr);
		/* Accept the incoming connection on listenfd */
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *) &clientlen);
		/* Read request into reqBuf */ 
		reqLen = readRequest(connfd, reqBuf, 0);
		printf("REQ BUF AFTER READ: %s\n", reqBuf);
		/* Parse reqBuf to get uri */
		sscanf(reqBuf, "%s %s", method, uri);
		/* If request is a PUT or POST, fetch the body of the request */
		if(strcmp(method, "PUT") == 0 || strcmp(method, "POST") == 0)
		{
			/* Find Content-Length in reqBuf */
			contentLength = strstr(reqBuf, "Content-Length: ");
			sscanf(contentLength, "%*s %d", &contLen);
			strcat(reqBuf, "\r\n");
			reqLen += readRequest(connfd, &reqBuf[strlen(reqBuf) - 1], contLen);
		}
		/* Parse uri into host, path and port */
		parse_uri(uri, hostname, pathname, &serverPort);
		/* Passover hosts that fail to be parsed by the parse_uri function */
		if(strlen(hostname) <= 0)
			continue;
		/* Connect to socket on server denoted by hostname */
		serverfd = Open_clientfd(hostname, serverPort);
		/* Write request to server */
		writeStuff(serverfd, reqBuf, reqLen);
		printf("REQ BUF AFTER WRITE: %s\n", reqBuf);
		/* Read response from server */
		respLen = readResponse(serverfd, respBuf);
		printf("RESP BUF AFTER READ: %s\n", respBuf->respContent);
		/* Close server connection */
		Close(serverfd);
		/* Check for disallowed words */
		i = 0;
		int foundDisallowed = 0;
		while(disallowedWords[i] != NULL && !foundDisallowed)
		{
			if(strstr(respBuf->respContent, disallowedWords[i]) != NULL)
			{
				writeStuff(connfd, disallowedPage, strlen(disallowedPage));
				foundDisallowed = 1;
			}
			i++;
		}
		if(!foundDisallowed)
			writeStuff(connfd, respBuf->respContent, respLen);
		printf("RESP BUF AFTER WRITE: %s\n", respBuf->respContent);
		/* Clear out respBuf->respContent */
		memset(respBuf->respContent, '\0', respBuf->respCeiling); 
		Close(connfd);
		/* Write to proxy.log */
		format_log_entry(proxyLog, &clientaddr, uri, respLen, foundDisallowed);
		fflush(proxyLog);
	}
	fclose(proxyLog);
	free(respBuf);
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
void format_log_entry(FILE * proxyLog, struct sockaddr_in *sockaddr, 
		      char *uri, int size, int blocked)
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

	int bytesWritten;
    /* Write the formatted log entry string to logfile */
	if(size == 0)
	{
		bytesWritten = fprintf(proxyLog, "%s: %d.%d.%d.%d %s NOTFOUND\n", time_str, a, b, c, d, uri);
		return;
	}
	if(blocked)	
    	bytesWritten = fprintf(proxyLog, "%s: %d.%d.%d.%d %s %d (BLOCKED: Page has disallowed words)\n", time_str, a, b, c, d, uri, size);
	else
		bytesWritten = fprintf(proxyLog, "%s: %d.%d.%d.%d %s %d\n", time_str, a, b, c, d, uri, size);

	if(!bytesWritten)
	{
		perror("COULD NOT WRITE TO proxy.log");
		exit(-1);
	}else
		return;
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
		Rio_writen(connfd, buf, n);
	}
}

int readRequest(int connfd, char * buf, int contentLength)
{
	size_t n;
	int totalBytes = 0;
	rio_t rio;
	Rio_readinitb(&rio, connfd);
	/* Set maximum number of bytes to read */
	contentLength = (contentLength ? contentLength : MAXLINE);
	while((n = Rio_readlineb(&rio, &buf[totalBytes], MAXLINE)) > 0 && totalBytes <= contentLength)
	{
		if(strcmp(&buf[totalBytes], "\r\n") == 0)
		{
			totalBytes += n;
			break;
		}
		totalBytes += n;
		if(totalBytes > MAXLINE)
		{
			printf("You have overrun your buffer");
			exit(-1);
		}
	}
	return totalBytes;
}

int readResponse(int connfd, RespBuf * respBuf)
{
	size_t n;
	int totalBytes = 0;
	char buf[MAXNET];
	rio_t rio;
	Rio_readinitb(&rio, connfd);
	while((n = Rio_readnb(&rio, buf, MAXNET)) > 0)
	{
		totalBytes += n;
		while(totalBytes > respBuf->respCeiling)
		{
			/* Double size of respBuf */
			char * temp;
			if((temp = malloc(respBuf->respCeiling)) == NULL)
			{
				perror("Out of memory.");
				exit(-1);
			}
			temp = respBuf->respContent;
			respBuf->respCeiling *= 2;
			if((respBuf->respContent = malloc(respBuf->respCeiling)) == NULL)
			{
				perror("Out of memory.");
				exit(-1);
			}
			memcpy(respBuf->respContent, temp, respBuf->respCeiling/2);
			free(temp);
		}
		/* Copy buf to the response struct */
		memcpy(&(respBuf->respContent[totalBytes-n]), buf, n);
	}
	return totalBytes;
}

void writeStuff(int connfd, char * buf, int contentLength)
{
	Rio_writen(connfd, buf, contentLength);
}
