id: 319051066
name: najeeb ibraheem
exercise name: HTTP Proxy Server

submited files:
1. threadpool.c: the threadpool implimintation file.
2. proxyServer.c: the server file.

the program takes as input: 1. the servers port, 2. max number of requests 3. number of threads 4. the path to the filter file

the program setups a server and waits for clients to send requests, whenever a client send a request the programs searchs for the requested files on the local machine
if it finds it the program sends the files in a HTTP response to the client.
If the files arent of the machine,  the program connects th the server of the host in the client's request and sends a request to that server requesting the files that the client wants
then the program sends a HTTP response to the client with the requested file and if the response is 200 OK the files will be saved on the machine
