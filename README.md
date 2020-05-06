# Client - Server connection

This is an implementation of a connection between server (or many servers) to one client 
under "infiniband" connection. [Click](https://en.wikipedia.org/wiki/InfiniBand) for more information.

The code enables the client to send messages or files to the server and to get it back later when needed. 
Depend on the amount of servers which are available, the messages or the files will be evenly saved among the servers. 
(For this to happen, one of the servers has to handle the task of distribute the files, it does not has to be a different server)

Inside "http-server" there is an implementation example of http-server using this functionality. 


#### Thanks:
troydhanson - for the great 'uthash' [lib](https://github.com/troydhanson/uthash).
