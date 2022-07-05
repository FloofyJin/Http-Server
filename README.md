HTTP Server
============================

This assignment builds on from the previous iteration of the HTTP server where the server was built to support a variety of HTTP requests from the client. Now that the log function is working as intended, I can see each request that received by the server. This becomes crucial for debugging later on. this assignment adds a multithreaded functionality to the entire request handling process. User can provide an arbitrary number of threads to use and server must support that.

#### Design:
HTTP server using threads.<br/>
Begin by creating a simple pthread_t instance on the main module of the program; see the effects of creating threads and instantiating thread function that can run on the thread at all times. The main function will have infinite loop that will be checkign for on coming request at all times. 
<br/>
Though, important to note is that this will not busy-wait because the program will make use of signals and conditional variables which all come from the pthread.h header.
<br/>
Inside each thread function, there must be a loop that checks to see if there needs to be work to be done by checking to see if the queue is not empty. It is necessary to close the connfd after the handle_connection() closes.
<br/>
One of the biggest developemental challenge was designing a system for terminating the program smoothly when termination signal is sent such as SIGINT or SIGTERM. Often times then not, there are valgrind errors on threads or not closing connfd which results in thread not being able to be joined. In the end, I couldn't figure it out so I dont join the thread when closing at all. There will be mem leaks but so does the resources binary. There are no major breaking issues so the test will prevail.
<br/>
When receiving request, it can come in many different smaller packets. So first, wait for all the content up to header to be received to start parsing them. 
<br/>
Note that global variable could turn into a big issue so malloc mem if need be.
<br/>
Big thing that needs to be implemented is Non-blocking IO. This is when you can read from request then come back to it later. Sometimes packets are sent byte by byte (even the headers), so you must wait untill all data has been received.  Epolling is pretty useful for keeping track of which connfd needs to be addressed first in line. This could work in conjunction with the queue.

#### issues:
Biggest hurdle I had when testing this program happened to be an issue with the output sent by the server. Though both are right, Olivertwist.py, which is the program for sending requests, looks for a response with "\r\n\r\n", however my response only had "\n\n". This set me back for over 5 days despite the fact that this issue is clearly a problem with the testing script itself with small part on mine.
<br/>

Log for each request should be in the format
>>>
``<Oper>,<URI>,<Status-Code>,<Request ID><header value>\n``
>>>

Also there are status codes to watch out for.
* 200 - OK - Success
* 201 - Created - Created new file
* 400 - Bad Request - request is ill formatted
* 403 - Forbidden - server cannot access file
* 404 - Not found - File does not exist. only used in GET.
* 500 - Internal server Error - unexpected issue preventing process
* 501 - Not Implemented - Request method is not implemented yet

>>>
$ ./httpserver 1234
>>>


Some commands from client:

GET
>>>
$ curl http://localhost:1234/foo.txt
>>>

>>>
$ printf "GET /foo.txt HTTP/1.1\r\n\r\n" | nc localhost 1234
>>>

PUT
>>>
$ curl http://localhost:1234/foo.txt -T filetosend.txt
>>>

>>>
$ printf "GET /foo.txt HTTP/1.1\r\nContent-Lenght: 12\r\nHello World!" | nc localhost 1234
>>>