#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "threadpool.h"
#include <netdb.h>
#include <arpa/inet.h>
#define MAXLINE 4096
#define WRITE_ERROR 0
#define OK 200
#define BAD_REQUEST 400
#define FORBIDDEN 403
#define NOT_FOUND 404
#define INTERNAL_ERROR 500
#define NOT_SUPPORTED 501
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
char* pathToFilter;
int dispatch_func(void*);
char *get_mime_type(char*);
void sendError(int sock,char* protocol,int code);
int checkIfIsNum(char*);
int checkIfHostInFilter(FILE*,char*);

int mkdir_p(const char *dir, const mode_t mode) {
    char tmp[256];
    char *p = NULL;
    struct stat sb;
    size_t len;
    
    //copy path 
    len = strnlen (dir, 256);
    if (len == 0 || len == 256) {
        return -1;
    }
    memcpy (tmp, dir, len);
    tmp[len] = '\0';

    //remove trailing slash 
    if(tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    //check if path exists and is a directory 
    if (stat (tmp, &sb) == 0) {
        if (S_ISDIR (sb.st_mode)) {
            return 0;
        }
    }
    
    //recursive mkdir
    for(p = tmp + 1; *p; p++) {
        if(*p == '/') {
            *p = 0;
            //test path 
            if (stat(tmp, &sb) != 0) {
                //path does not exist - create directory 
                if (mkdir(tmp, mode) < 0) {
                    return -1;
                }
            } else if (!S_ISDIR(sb.st_mode)) {
                //not a directory
                return -1;
            }
            *p = '/';
        }
    }
    //test path
    if (stat(tmp, &sb) != 0) {
        //path does not exist - create directory
        if (mkdir(tmp, mode) < 0) {
            return -1;
        }
    } else if (!S_ISDIR(sb.st_mode)) {
        //not a directory 
        return -1;
    }
    return 0;
}

int main(int argc, char *argv[])
{ 
       
    int listenSock,newSock,count =0;
    int *clientSock;
    struct sockaddr_in my_server, clientAddr;
    socklen_t sockLen = sizeof(struct sockaddr_in);
    if(argc != 5)
    {
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>");
        return(EXIT_FAILURE);
    }
    int port, size, maxRequests;
    if((checkIfIsNum(argv[1])<0)||((checkIfIsNum(argv[2])<0))||((checkIfIsNum(argv[3])<0)))//check the input
    {
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>");
        return(EXIT_FAILURE);
    }
    port = atoi(argv[1]);
    size = atoi(argv[2]);
    maxRequests = atoi(argv[3]);
    pathToFilter = (char*)malloc(strlen(argv[4]));
    strcpy(pathToFilter,argv[4]);
    if(pathToFilter == NULL)
    {
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>");
        return(EXIT_FAILURE);
    }
    if(port < 0 || size > MAXT_IN_POOL || size < 0 || maxRequests < 1)
    {
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>");
        return(EXIT_FAILURE);
    }
    threadpool *tpool = create_threadpool(size);
    if(tpool == NULL)
    {
        printf("Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>");;
        return(EXIT_FAILURE);
    }
    bzero((char*)&my_server,sizeof(struct sockaddr_in));
    (&my_server)->sin_family = AF_INET;
    (&my_server)->sin_addr.s_addr = INADDR_ANY;
    (&my_server)->sin_port = htons(port);
    listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock < 0)
	{
		perror("socket");
		destroy_threadpool(tpool);
		return(EXIT_FAILURE);
	}
    if(bind(listenSock,(struct sockaddr*)(&my_server), sizeof(struct sockaddr_in)) < 0)
    {
        perror("bind");
		close(listenSock);
		destroy_threadpool(tpool);
		return(EXIT_FAILURE);
    }
    if (listen(listenSock, 5) < 0)
	{
		perror("listen");
		close(listenSock);
		destroy_threadpool(tpool);
		return(EXIT_FAILURE);
	}
    while(count < maxRequests)
	{
		bzero((char*)&clientAddr, sizeof(struct sockaddr_in)); //init 0 for each client
		// accept will initialize a new socket to the client's request
		newSock = accept(listenSock, (struct sockaddr*)&clientAddr, &sockLen);
		//the server is running
		if (newSock < 0) //don't shutdown over one socket that is unsuccsesful
			perror("opening new socket\n");
		else
		{	//let the threads know that there is a new request
			clientSock = (int*)calloc(1, sizeof(int));
			if (!clientSock)
				perror("error: <sys_call>\n");
			else
			{
				*clientSock = newSock;
				dispatch(tpool, dispatch_func, (void*)clientSock);
				count++;
			}
		}
        
	}
    close(listenSock);
	destroy_threadpool(tpool);
	return 0;
}
//this function gets the filter file FD and host and checks if the host is in the filte
// the function return 1 if it isn't in it, 0 if it is and -1 if there was an error
int checkIfHostInFilter(FILE* fileFD,char* host)
{
    printf("%s",host);
    struct hostent *Hostent = gethostbyname(host);
    if(Hostent == NULL)
        return -1;
    char* IP = (char*)malloc(16);
    struct in_addr addr = *(struct in_addr * )(Hostent->h_addr_list[0]);
    strcpy(IP,inet_ntoa(addr));//get the ip of the host
    char* bin32 =(char*)malloc(32);
    char s[2] = ".";
    char* tok2 = (char*)malloc(3*sizeof(char));
    char* temp = (char*)malloc(strlen(IP));
    strcpy(temp,IP);
    tok2 = strtok(temp,s);
    
    while(tok2 != NULL)//convert the ip to binary
    {
        char bin8[]  = "00000000";
        int dec = atoi(tok2);
        for (int pos = 7; pos >= 0; --pos)
        {
            if (dec % 2) 
                bin8[pos] = '1';
            dec /= 2;
        }
        strcat(bin32,bin8);
        tok2 = strtok(NULL,".");
    }
    
    char c = IP[0];
    if(c > '9' ||c<'0' || strstr(IP,".") == NULL)
        return -1;
   
    if(Hostent == NULL)
    {
        return -1;
    }
    char* str = (char*)malloc(32*sizeof(char));
    char* token = (char*)malloc(32*sizeof(char));
    char s2[2] ="/";
    
    while(fgets(str,32,fileFD))
    {
        str[strcspn(str, "\r\n")] = 0;
        char c = str[0];
        if(c > '9' || c < '0' )
        {
            if(strcmp(str,host) == 0)
                return 0;
        }
        else
        {
            token = strtok(str,s2);
            char* address =(char*)malloc(32*sizeof(char));
            strcpy(address,token);
            token = strtok(NULL,s2);
            int numOfBits = atoi(token);
            char* addressBin32 = (char*)malloc(32);
            char* tok3 = (char*)malloc(3*sizeof(char));
            tok3 = strtok(address,s);
            while(tok3 != NULL)//convert the ip to binary
            {
                char bin8[]  = "00000000";
                int dec = atoi(tok3);
                for (int pos = 7; pos >= 0; --pos)
                {
                    if (dec % 2) 
                        bin8[pos] = '1';
                    dec /= 2;
                }
                strcat(addressBin32,bin8);
                tok3 = strtok(NULL,".");       
            }
            if(strncmp(bin32,addressBin32,numOfBits) == 0)//compare the firs X bits of the binary ips
                return 0;
            
            
        }
    }
    
   return 1; 
}
int dispatch_func(void *arg)
{
    
    int socketFD = *((int*)arg);
    int code = 0;
    static char request[MAXLINE];
    char protocol[9] = {"HTTP/1.0"};
    char *headerCheck = NULL;
    int bytesRead;
    int size = 0;
    char tmp[MAXLINE] ={"\0"};
    
    
    while(1)
    {
        bzero(tmp, sizeof(tmp));
        bytesRead = read(socketFD,tmp,sizeof(tmp));
        
        if (bytesRead < 0){
			perror("read");
			close(socketFD);
			return -1;
		}
        else if (bytesRead > 0)
        {
            size += bytesRead;
            strncat(request,tmp,strlen(tmp));

            if (strstr(request, "\r\n\r\n") != NULL) //found "\r\n"
				break;
            
        }
        else
			break;
    }
    request[size+1] = '\0';
    if (strlen(request) == 0)//invalid request
	{
		close(socketFD);
		return -1;
	}
    printf("HTTP request =\n%s\nLEN = %d\n", request, strlen(request));
    headerCheck = strstr(request,"\r\n");//invalid request
    if(headerCheck == NULL)
    {
        sendError(socketFD,protocol,BAD_REQUEST);
        close(socketFD);
        return -1;
    }
    if ((strstr(request, "HTTP/1.0") == NULL && strstr(request, "HTTP/1.1") == NULL) || (strstr(request, "HTTP/1.0") != NULL && strstr(request, "HTTP/1.1") != NULL))
    {
        sendError(socketFD,protocol,BAD_REQUEST);//bad request
        close(socketFD);
        return -1;
    }
    if (strstr(request, "HTTP/1.0") != NULL) //first version (1.0)
	{
		strcpy(protocol, "HTTP/1.0");			
	}
	else if (strstr(request, "HTTP/1.1") !=NULL) //second version (1.1)
	{
		strcpy(protocol, "HTTP/1.1");
	}
    if(strstr(request,"GET ") == NULL)//methode not supported
    {
		sendError(socketFD, protocol, NOT_SUPPORTED);
		close(socketFD);
		return -1;
    }
    if(strstr(request,"Host:") == NULL)//
    {
		sendError(socketFD, protocol, BAD_REQUEST);
		close(socketFD);
		return -1;
    }
    
    char* path = (char*)malloc(100);//seperate the request to extrac the host , port, protocol and path
    memset(path,0,100);
    char p[100] = {0};
    char host[100] = {0};
    sscanf(request,"GET %s %s\r\nHost: %s\r\n",path,p,host);
    
    
    
    FILE* fd = fopen(pathToFilter , "r");
    if(fd == NULL)
    {
        sendError(socketFD,protocol,NOT_FOUND);
        close(socketFD);
        return -1;
    }
    
    
    int checkFilter = checkIfHostInFilter(fd,host);//check if the host is in the filter
    if(checkFilter == -1)//error occured
    {
        sendError(socketFD,protocol,NOT_FOUND);
        close(socketFD);
        return -1;
    }
    else if (checkFilter == 0)//host in filter
    {
       sendError(socketFD,protocol,FORBIDDEN);
        close(socketFD);
        return -1;
    }
    if(strcmp(path,"/") == 0)
    {
        close(socketFD);
        return -1;
    }
    //otherwise continue
    char* workingDir= (char*)malloc(256);
    workingDir = getwd(workingDir);//get the current working directory
    
    char * realPath = (char *) malloc(strlen(path)+strlen(host)+4+strlen(workingDir)+50);
    char* fileName;
    
    if(strcmp(path+strlen(path)-1,"/")==0)//if the path ends woth / add index.html to the end
    {
        strcpy(realPath,workingDir);//create the real path ../../Ex3./..........etc
        strcat(realPath,"/");
        strcat(realPath,host);
        strcat(realPath,"/");
        strcat(realPath,path);
        fileName = (char*)malloc(strlen("index.html"));
        strcpy(fileName,"index.html");
        path = realloc(path,strlen(path)+strlen("index.html"));
        strcat(path,"index.html");
    }
    else
    {
        int index = 0;
        for(int i = strlen(path)-1;i>=0;i--)//fine the index to the last /
        { 
            if(path[i] == '/')
            {
                index = i;
            break;
        }
      }
      
      strcpy(realPath,workingDir);
      strcat(realPath,"/");
      strcat(realPath,host);
      int size = strlen(realPath);
      for(int i=0;i<index;i++)//copy the directory path
      {
        realPath[size+i] = path[i];
      }
      fileName = (char*)malloc(strlen(path)-index);
      int j=0;
      for(int i = index;i<strlen(path);i++)//get the name of the file we neeed to create
      {
        fileName[j] = path[i];
        j++;
      }
    }
    
    char* fileFullPath = (char*)malloc(strlen(realPath)+strlen(fileName));
    strcpy(fileFullPath,realPath);
    strcat(fileFullPath,fileName);//the full path of the file we need to creat
    
    int file = open(fileFullPath,O_RDONLY,0777);
    if(file != -1)//if the file exists
    {
        struct stat info;
        stat(fileFullPath,&info);
        char* res = (char*)malloc(100+info.st_size);
        char* type = get_mime_type(path);
        sprintf(res,"%s 200 OK\r\nContent-Length: %ld\r\nContent-type: %s\r\nconnection: Close\r\n\r\n",p,info.st_size,type);//the response we send back to the socket
        if (write(socketFD, res, strlen(res)) < 0)
        {
            perror("write");
            close(file);
            sendError(socketFD,protocol,0);
            close(socketFD);
            return -1;
        }
        char unsigned buf[4001];
        buf[4001] = '\0';
        int BytesRead =0;
        while (1)
		{
            bzero(buf,4000);
            buf[4001] = '\0';
			//BytesRead - size of what was read
			BytesRead = read(file,buf,4000);;
			if (BytesRead == 0) //no more stuff to be read
			{
				break;
			}
			else if (BytesRead > 0) //nothing to read
			{	//write the file to socket
				if (write(socketFD, buf, BytesRead) < 0)//write error
				{
					perror("write");
					code = 0;
					close(file);
					if (code != WRITE_ERROR)
						sendError(socketFD, protocol, code);
					close(socketFD);
					return -1; 
				}
                printf("File is given from local filesystem\n");
			}
			else //reading error
			{
				perror("read");
				close(file); 
				sendError(socketFD, protocol, INTERNAL_ERROR);
				close(socketFD);
				return -1;
			}
		}
	//	write (socketFD, "\r\n\r\n", 4);//finish the response and send it to socket
		close(file);
        close(socketFD);
        return 0;
    }
    else
    {
        struct hostent *server;//initilize the server
        int serverSock = socket(AF_INET,SOCK_STREAM,0);
        if(serverSock < 0)
        {
            sendError(socketFD, protocol, INTERNAL_ERROR);
			close(socketFD);
			return -1;
        }
        server = gethostbyname(host);
        if(server == NULL)
        {
            sendError(socketFD, protocol, NOT_FOUND);
			close(socketFD);
			return -1;
        }
        char buf[4096];
        struct sockaddr_in serverAddr;
        serverAddr.sin_port = htons(80);
        bcopy((char *)server->h_addr, (char *)&serverAddr.sin_addr.s_addr, server->h_length);
        serverAddr.sin_family = AF_INET;
        int rc = connect(serverSock,(const struct sockaddr*)&serverAddr, sizeof(serverAddr));
        if(rc < 0)
        {
            sendError(socketFD, protocol, INTERNAL_ERROR);
			close(socketFD);
			return -1;
        }
        bzero(buf,strlen(buf));
        sprintf(buf,"GET %s %s\r\nHost: %s\r\n\r\n",path,protocol,host);//make the get request
        write(serverSock,buf,strlen(buf));//send the request to the server
        int saveFlag = 1;
        int fileCreated = 1;
        int fileindex = -1;
        int fileFD;
        int responseSize = 0;
        char unsigned rbuf[4001];
        rbuf[4001] = '\0';
        while(1)
        {
            
            bzero(rbuf,4000);
            rbuf[4001] = '\0';

            int r_c = read(serverSock, rbuf,  4000);

            if(strstr(rbuf,"200 OK")!= NULL && saveFlag == 1)
            {
                saveFlag = 0;
                if (-1 == mkdir_p(realPath, 0700))
                {   
                    perror("mkdir_p failed\n") ;
                    exit(EXIT_FAILURE);
                }
                fileFD = open(fileFullPath , O_CREAT | O_RDWR , 0777);
                fileCreated = 0;
            }
            if(r_c > 0)
            {//print the response
                if (write(socketFD, rbuf, strlen(rbuf)) < 0)//write error
		        {
			        perror("write");
			        code = 0;
			        close(file);
			        if (code != WRITE_ERROR)
                    {
				        sendError(socketFD, protocol, code);
				        close(socketFD);
				        return -1; 
			        }
                }
                responseSize +=strlen(rbuf);
            }
            printf("before");
            if (r_c == 0)
            {
                printf("in");
                printf("File is given from origin server\n");
                break;
            }
            printf("after");
            if (r_c < 0)
            {
                ("read() failed\n") ;
                exit(EXIT_FAILURE);

            }
            if(fileCreated == 0)
            {
                if(strstr(rbuf,"HTTP/1.1 200 OK")!= NULL || strstr(rbuf,"HTTP/1.0 200 OK")!= NULL)
                {
                    for(int i=0;i< 4096-3;i++)
                    {
                        if((rbuf[i] == '\r')&&(rbuf[i+1] == '\n')&&(rbuf[i+2] == '\r')&&(rbuf[i+3] == '\n'))
                            fileindex = i + 4;
                    }
                    write(fileFD,rbuf+ fileindex,r_c-fileindex);
                }
                else{
                    write(fileFD,rbuf,r_c);
                }            
            }
            
        }
        

        close(serverSock);
        close(fileFD);
        close(socketFD);
        return 0;
    }
    close(socketFD);
    return 0;
}

int checkIfIsNum(char* str)
{
    for(int i=0;i<strlen(str);i++)
        if(str[i] > '9' || str[i] < '0')
            return -1;
    return 0;
}

void sendError(int sock,char* protocol,int code)
{
    
    char* ResponseHtmlCode = (char*) malloc(512);
    char* ResponsehtmlHeader = (char*) malloc(512);
    char* Response = (char*) malloc(1024);


    
    if(code == 400)
    {
        sprintf(ResponsehtmlHeader,"HTTP/1.0 400 Bad Request\r\nContent-Type: text/html\r\n");
        sprintf(ResponseHtmlCode, "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n<BODY><H4>400 Bad request</H4>\nBad Request.\n</BODY></HTML>\r\n\r\n");
    }
    if(code == 403)
    {
        sprintf(ResponsehtmlHeader,"HTTP/1.0 403 Forbidden\r\nContent-Type: text/html\r\n");
        sprintf(ResponseHtmlCode, "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\r\n<BODY><H4>403 Forbidden</H4>\nAccess denied.\n</BODY></HTML>\r\n\r\n");
    }
    if(code == 404)
    {        
        sprintf(ResponsehtmlHeader,"HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n");
        sprintf(ResponseHtmlCode, "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\r\n<BODY><H4>404 Not Found</H4>\nFile not found.\n</BODY></HTML>\r\n\r\n");
    }
    if(code == 501)
    {        
        sprintf(ResponsehtmlHeader,"HTTP/1.0 501 Not supported\r\nContent-Type: text/html\r\n");
        sprintf(ResponseHtmlCode, "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\r\n<BODY><H4>501 Not supported</H4>\nMethod is not supported.\n</BODY></HTML>\r\n\r\n");
    }
    if(code == 500)
    {
        sprintf(ResponsehtmlHeader,"HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/html\r\n");
        sprintf(ResponseHtmlCode, "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n<BODY><H4>500 Internal Server Error</H4>\nSome server side error.\n</BODY></HTML>\r\n\r\n");
    }

    

    sprintf(ResponsehtmlHeader + strlen(ResponsehtmlHeader),
		"Content-Length: %lu\r\nConnection: close\r\n\r\n", strlen(ResponseHtmlCode));

    strncat(Response,ResponsehtmlHeader,strlen(ResponsehtmlHeader));
    strncat(Response,ResponseHtmlCode,strlen(ResponseHtmlCode));

    if(write(sock,Response,strlen(Response))<0)
        perror("write");


    
}



char *get_mime_type(char *name)
{
    char *ext = strrchr(name, '.');
    if (!ext) return NULL;
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) 
        return "text/html";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) 
        return "image/jpeg";
    if (strcmp(ext, ".gif") == 0) 
        return "image/gif";
    if (strcmp(ext, ".png") == 0) 
        return "image/png";
    if (strcmp(ext, ".css") == 0) 
        return "text/css";
    if (strcmp(ext, ".au") == 0) 
        return "audio/basic";
    if (strcmp(ext, ".wav") == 0) 
        return "audio/wav";
    if (strcmp(ext, ".avi") == 0) 
        return "video/x-msvideo";
    if (strcmp(ext, ".mpeg") == 0 || strcmp(ext, ".mpg") == 0) 
        return "video/mpeg";
    if (strcmp(ext, ".mp3") == 0) 
        return "audio/mpeg";
    return NULL;

}