# MultithreadedWS
A simple multi-threaded web server in C++ with support for thread pooling. Currently supports GET requests.

### Overview -

A mutli-threaded web server solves the scalability issues that plague single-threaded web server. Since thread-creation and deletion is expensive in terms of computational resources that are used, maximum performance can be obtained if we use a thread-pooled approach. In this, a main thread creates a fixed number of "worker" threads. The main thread continuously listens and accepts incoming connections. Whilst doing so, it creates request objects and places them into a data structure. The worker thread then pulls requests from the buffer according to the scheduling policy and produces the necessary response. Thus, the main thread can accept connections as long as memory is available, while a fixed number of threads churn away at maximum efficiency.

### Features

At the moment this web server can accomplish the following things:

	1) If the user knows the path to a particular file from the configured root_directory, the server can serve the requisite file once it receives a GET "filepath" request.
	
	2) It can list the contents of a specified directory, sorted by filename lexicographically. 
	
	3) If the specified file/dir does not exist, it sends a 404 Not Found message to the client.
	
	4) Only GET requests are supported currently. Any other request will be responded with a 405 - Method Not Allowed response.
	

### Usage

Compile the program in C++ as "g++ server.cpp -pthread -o server".

Once it is succesfully compiled you can run it - "./server [−h] [−p portnumber] [−r path_to_rootdir] [−n number_of_activethreads]"

			"-h"          : Print help list of available command line arguments.
	        "−p port      : Change port. Default is 5002.
	        "−r dir       : Change root directory. Please ensure that the root directory ends with a slash "/"
	        "−n number    : Set number of threads.

The server can respond to the GET requests via a web browser or telnet. 

### Compatibility.
I have tested this on OS X 10.12 (Sierra). However it may react strangely on a Windows machine. For example _stat function which I've used is not native to Windows, and may cause unspecified behavior.

### Implementation:

Scheduling Policy: There are many policies that can be used such as First Come First Served, Shortest Job First, etc. Each scheduling policy has their own pros and cons. Shortest Job First, for instance, tries to approximate a particular job might require, and then execute the process. However, there is a possibility of "starvation" in which some jobs may never get the chance to execute. 

For this task, I decided to use FCFS because of three reasons - 

	(1) It is simple to implement
	
	(2) Does not cause starvation
	
	(3) At the moment since only limited types of requests are supported, FCFS will serve our purpose quite well

Data Structures: I've made use of C++ Standard Template Library. In particular, I've made use of std::<queue> data structure which is basically a container that easily allows us to use it for common queue operations such as pushing an object into back of queue and retrieving the element from the front of the queue.

Also made use of std::set<>, which is a container in C++ STL implemented internally as a Binary Search Tree -- I used this to sort the filenames in the directory.

I've used an image from University of Nortre Dame's webpage (1) which serves as the basic working architecture for the project, the image is included in this folder and link to that specific tutorial is listed below in the references section.

I've attempted to modularize the code as much as possible. In hindsight an OOP paradigm might suit this project better if we were to extend it with more functionalities. I'd be glad to re-work on this aspect if required.


### Mutex and Condition variables:

Since multiple threads use the shared memory space, we need to ensure that they do not use each others' memory space. For this, I've implemented mutex locks in various sections of the program. Namely, the ReadyQ function which is responsible for pushing new request structs into the queue, the scheduler function which is responsible for taking elements one by one from the queue and the execR function which is responsible for serving the request to the client. 
A signal is also sent between scheduler() and execR() function and between readyQ and scheduler function. The purpose of signal (pthread_cond_signal) is to ensure that they wait long enough in absence of any valid requests, and to synchronize and to ensure that updated information is sent to the correct places so that execution is not stalled/program does not end for reasons like lack of elements in the queue.(2)

### Utility Functions: 

void help_user();
	This function displays the list of available command line arguments that can be used for running the server

int DirOrFile(string filename);
	This function takes as input a string, and tries to evaluate as to whether it is a file, a folder or neither. It returns 1 if the string is a directory. Returns 2 if it is a file, else returns 3.

void show_dir_content();
	This function takes a directory path, and pushes all the directory contents into a std::set<> container. which automatically sorts the content based on filename. Then, a simple iterator is used to iterate over all the elements in the  directory.

void filesize();
	Obtains the filesize of the given file, and modifies the client request structure with that data.

string gettime();
	Obtains the current time in the format required for the HTTP header.

string getcontenttype();
	Identifies whether a requested file is a jpg/png/gif image and sends appropriate header strings

int createSocket() and void sigchld_handler();
	Creates a server socket and handles the errors accordingly. Code adapted from Beej's Guide to Network Programming [3].

void queueProcess();
	This section continusouly listens for incoming connections from clients. Once it receives the data, the request is parsed, relevant data like request type and details is packaged into a structure "clientreq". This is pushed into a ready-queue.

void readyQ();
	The ready-queue is used to send request packets to the scheduler. 

void scheduler();
	The scheduler picks request-structs from the ready queue in FCFS order, i.e. items from the front of the queue are retrieved and a signal is sent to execR to process the extracted element. The extracted element is then popped from the queue. 

void execR();
	execR processes the client request and sends the response to the client.

void serveGET();
	If the request is for a file, it sends the file. If it is for a directory, it sends directory content listing. Else, it sends a 404 message to the client.

void send_file();
	Sends the requested file to user. 

void send_dir();
	Sends an HTML header to the client and calls the function show_dir_content() to fetch directory contents.

void send_404();
	Sends a 404 - File/Dir not found message to the client.

void send_405();
	Sends a 405 - Method Not Allowed message if the client tries to make a PUT/POST or any other request.


### References:

[1] https://www3.nd.edu/~dthain/courses/cse30341/spring2009/project4/project4.html - for overview and general understanding of the architecture of the multi-threaded server.

[2] http://users.cs.cf.ac.uk/Dave.Marshall/C/node31.html#SECTION003100000000000000000 - Tutorial for Mutexs

[3] http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html - Code for createSocket() function is adapted from here.



