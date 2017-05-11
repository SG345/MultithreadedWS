#include	  <iostream>
#include	  <map>
#include    <set>
#include    <queue>
#include    <string>
#include    <sstream>
#include    <fstream>

#include	  <pthread.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <dirent.h>
#include    <ctype.h>
#include 	  <syslog.h>
#include	  <unistd.h>
#include 	  <termios.h>
#include	  <fcntl.h>
#include	  <assert.h>
#include	  <netdb.h>
#include	  <inttypes.h>
#include	  <time.h>
#include    <errno.h>
#include    <signal.h>
#include    <dirent.h>
#include 	  <arpa/inet.h>
#include	  <netinet/in.h>
#include	  <sys/types.h>
#include	  <sys/socket.h>
#include 	  <sys/stat.h>
#include    <sys/wait.h>

#define ERR_404     "<html><body><h1><font color=\"FF3333\"> Error 404 Not Found </font><h1><br><h3>The resource you were looking for was not found on this server</h3></body></html>"
#define ERR_405     "<html><body><h1><font color=\"FF3333\"> Error 405 Request Not Supported </font><h1><br><h3>The request you made is not supported by this server</h3></body></html>"
#define SERVER_NAME "Sushrut"
#define HTTP_OK     "HTTP/1.0 200 OK\n"
#define HTTP_NOTOK  "HTTP/1.0 404 Not Found\n"
#define HTTP_NOTALLOWED "HTTP/1.0 405 Method Not Allowed\n"
#define HTML        "Content-Type:text/html\n"

#define	BUF_LEN	100000
#define BUFSIZE 100000

using namespace std;

char *pvar, ch;
int portno = 5002, NThreads = 10, p;

pthread_mutex_t readQ_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t execT_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t block_cond_emptyQ = PTHREAD_COND_INITIALIZER;
pthread_cond_t block_cond_execT = PTHREAD_COND_INITIALIZER;

string rootdir = "/Users/sushrut/desktop/";

struct clientreq {
	int ID;
	string filename;
	string type;
	int filesize;
};

struct clientreq F;

queue<clientreq> Q;

void help_user() {
	fprintf(stderr, "usage:./server [−h] [−p portnumber] [−r path_to_rootdir] [−n number_of_activethreads]\n");
	fprintf(stderr,
	        "−h           : Print help list of available command line arguments.\n"
	        "−p port      : Change port. Default is 5002.\n"
	        "−r dir       : Change root directory. Please ensure that it ends with a slash.\n"
	        "−n number    : Set number of threads.\n"
	       );
	exit(1);
}


int DirOrFile (string filename) {

	struct stat s;

	if ( stat(filename.c_str(), &s) == 0) {
		if (s.st_mode & S_IFDIR)
			return 1; //it is a directory
		else if (s.st_mode & S_IFREG)
			return 2; //it is a file!
		else
			return 3; //um, it is neither a file nor a directory
	}
	return 3;
}

void show_dir_content (string path, int clientsocket) {

	string heading = "<html><h1> <br> Directory Listing </h1></html>\n \n ";
	send(clientsocket, heading.c_str(), strlen(heading.c_str()), 0	);

	set<string> dirlist;
	DIR * d = opendir(path.c_str());
	if (d == NULL) return;

	struct dirent * dir;

	while ((dir = readdir(d)) != NULL)
		dirlist.insert(dir->d_name);

	closedir(d);

	for (set<string>::iterator it = dirlist.begin(); it != dirlist.end(); ++it)
	{

		string tempbuffer = "<br> <a href=\"" + (*it);
		std::size_t found = (*it).find_first_of(".");
		if (found == string::npos)
			tempbuffer += "/";
		tempbuffer += "\" >" + (*it) + "</a>" + "\n";

		send(clientsocket, tempbuffer.c_str(), strlen(tempbuffer.c_str()), 0);
	}

}

void filesize (clientreq c) {

	FILE *fd = fopen(c.filename.c_str(), "rb");
	fseek(fd, 0, SEEK_END);
	if (ferror(fd)) {
		c.filesize = 0;
	}
	c.filesize = ftell(fd);
	fclose(fd);
}

string gettime() {

	char timeStr[100];
	time_t ltime;
	char timebuf[9];
	struct stat st_buf;
	ltime = st_buf.st_mtime;
	struct tm *timebuffer = gmtime(&ltime);
	strftime(timeStr, 100, "%d-%b-%Y %H:%M:%S", timebuffer);
	string current_time = timeStr;
	return current_time;

}

string getcontenttype (string filename) {

	string CType = "";

	if (filename.find(".jpg") != std::string::npos)
		CType = "Content-Type:image/jpg\n";

	else if (filename.find(".gif") != std::string::npos)
		CType = "Content-Type:image/gif\n";

	else if (filename.find(".png") != std::string::npos)
		CType = "Content-Type:image/png\n";

	else
		CType = "Content-Type:text/html\n";

	return CType;

}

void sigchld_handler (int s) {
	// waitpid() might overwrite errno, so we save and restore it:
	int saved_errno = errno;

	while (waitpid(-1, NULL, WNOHANG) > 0);

	errno = saved_errno;
}

int createSocket() {

	int sockfd, newsockfd, clilen;
	char buffer[256];
	struct sockaddr_in serv_addr, cli_addr;
	struct sigaction sa;
	int  n;

	/* First call to socket() function */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("Error opening socket");
		exit(1);
	}
	/* Initialize socket structure */
	bzero((char *) &serv_addr, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	/* Now bind the host address using bind() call.*/
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
	         sizeof(serv_addr)) < 0)
	{
		perror("ERROR on binding");
		exit(1);
	}

	if (listen(sockfd, NThreads) == -1) {
		perror("listen");
		exit(1);
	}
	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	return sockfd;
}

void readyQ (struct clientreq C) {

	pthread_mutex_lock(&readQ_mutex);

	Q.push(C);

	pthread_cond_signal(&block_cond_emptyQ);
	pthread_mutex_unlock(&readQ_mutex);
}

void *queueProcess (void* sock) {

	int recvdata = 0, client_fd, socketfd, len;
	char buffer[102400];
	struct clientreq C;


	socketfd = *((int*)sock);
	struct sockaddr_in clientaddr;

	len = sizeof(clientaddr);

	while (1) {

		client_fd =  accept(socketfd, (struct sockaddr*) &clientaddr, (socklen_t *) &len);
		if (client_fd == -1)
			perror("server: accept");

		C.ID = client_fd;
		if ((recvdata = recv(client_fd, buffer, sizeof(buffer), 0)) <= 0) {
			close(client_fd);
			continue;
		}

		buffer[recvdata] = '\0';

		string fetchline(buffer);
		int current = 0;
		int next = fetchline.find_first_of("\r\n", current);
		string firstline = fetchline.substr(current, next - current);

		string parse = firstline;
		istringstream iss(parse);
		string token;

		getline(iss, token, '/');
		C.type = token;
		getline(iss, token, ' ');
		C.filename = token;

		C.filename = rootdir + C.filename;

		if (DirOrFile(C.filename) == 2) filesize(C);
		else C.filesize = 0;
		readyQ(C);


	}
}

void *scheduler (void*) {
	while (1) {

		pthread_mutex_lock(&readQ_mutex);

		while (Q.empty())
			pthread_cond_wait(&block_cond_emptyQ, &readQ_mutex);

		pthread_mutex_lock(&execT_mutex);

		F = Q.front();

		pthread_cond_signal(&block_cond_execT);
		pthread_mutex_unlock(&execT_mutex);

		Q.pop();

		pthread_mutex_unlock(&readQ_mutex);

	}
}

void send_file (clientreq c) {

	char filebuf[BUF_LEN];
	time_t timevar = time(NULL);
	sprintf(filebuf, "%s Date:%s SERVER:%s\n Last Modified:%s\n %s Content-Length:%d\n\n",
	        HTTP_OK, asctime(gmtime(&timevar)), SERVER_NAME, gettime().c_str(), getcontenttype(c.filename).c_str(), c.filesize);

	send(c.ID, filebuf, strlen(filebuf), 0);
	int fd = open(c.filename.c_str(), O_RDONLY, S_IREAD | S_IWRITE);

	int buffile = 1;
	while (buffile > 0) {
		buffile = read(fd, filebuf, BUFSIZE);
		if (buffile > 0) {
			send(c.ID, filebuf, buffile, 0);
		}
	}

}

void send_dir (clientreq c) {

	char buffer[BUF_LEN];
	sprintf(buffer, "%s Date:%s SERVER:%s\n %s Content-Length:%d\n\n",
	        HTTP_OK, SERVER_NAME, gettime().c_str(), HTML, c.filesize);
	send(c.ID, buffer, strlen(buffer), 0);
	show_dir_content(c.filename, c.ID);
}

void send_404 (clientreq c) {

	char buffer[BUF_LEN];
	sprintf(buffer, "%s Date:%s SERVER:%s\n %s Content-Length:%d\n\n",
	        HTTP_NOTOK, SERVER_NAME, gettime().c_str(), HTML, c.filesize);
	send(c.ID, buffer, strlen(buffer), 0);
	strcpy(buffer, ERR_404);
	send(c.ID, buffer, strlen(buffer), 0);

}
void send_405 (clientreq c) {

	char buffer[BUF_LEN];
	sprintf(buffer, "%s Date:%s SERVER:%s\n %s Content-Length:%d\n\n",
	        HTTP_NOTOK, SERVER_NAME, gettime().c_str(), HTML, c.filesize);
	send(c.ID, buffer, strlen(buffer), 0);
	strcpy(buffer, ERR_405);
	send(c.ID, buffer, strlen(buffer), 0);
}

void serveGET (clientreq c) {

	int response = DirOrFile(c.filename);

	if (response == 1)
		send_dir(c);
	else if (response == 2)
		send_file(c);
	else
		send_404(c);

}

void* execR (void*) {

	while (1) {

		pthread_mutex_lock(&execT_mutex);
		pthread_cond_wait(&block_cond_execT, &execT_mutex);

		struct clientreq c = F;

		if (c.type.find("GET") != std::string::npos)
			serveGET(c);
		else
			send_405(c);

		cout << "Processed Request: " << c.type << " " << c.filename << " for client: " << c.ID << "\n\n";

		close(c.ID);

		pthread_mutex_unlock(&execT_mutex);
	}
}



int main(int argc, char *argv[]) {

	if ((pvar = rindex(argv[0], '/')) == NULL)
		pvar = argv[0];
	else
		pvar++;

	while ((ch = getopt(argc, argv, "dhr:s:n:p:l:t:")) != -1)
		switch (ch) {
		case 'r':

			rootdir = optarg;

			if (DirOrFile(rootdir) != 1) {
				cout << "Root dir is not accessible. Please try some other directory" << endl;
				exit(1);
			}
			break;

		case 'n':

			NThreads = atoi(optarg);
			if (NThreads <= 0) {
				cout << "Please enter number of threads greater than 0" << endl;
				exit(1);
			}

			break;

		case 'p':
			p = atoi(optarg);

			if (p < 1024 || p > 9999) {
				cout << "Please enter port greater than 1024 and less than 9999" << endl;
				exit(1);
			}

			portno = p;

			break;

		case 'h': help_user();
		default:
			help_user();
			break;
		}

	argc -= optind;
	if (argc != 0)
		help_user();

	int sock;

	pthread_t queueT, schT, *execT;

	sock = createSocket();

	cout << "\n\nServer is running successfully\n" << "Root Directory: " << rootdir << "\nPort: " << portno << endl;
	execT = new pthread_t[NThreads];
	pthread_create(&queueT, NULL,  queueProcess, (void *) &sock);
	pthread_create(&schT, NULL, scheduler, NULL);
	for (int i = 0; i < NThreads; ++i) {
		pthread_create(&execT[i], NULL,  execR, NULL);
	}
	pthread_join(queueT, NULL);

	return 0;
}


