#include <sys/socket.h> // for connect, bind, listen, coskaddr struct, AF_INET and SOCK_STREAM
#include <netinet/in.h> //for htonl, htons, ntohl, ntohs, sockaddr_in struct
#include <arpa/inet.h> // for inet_aton and inet_ntoa
#include <sys/types.h> //various type definitions like fd_set used with the select() method
#include <unistd.h> //for the select function
#include <stdlib.h> //avoid warning when calling exit(0)
#include <stdio.h> //read, write from stdin and out
#include <string.h> //will be using strlen
#include <sys/select.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>

#define BUF_SIZE 1000000
#define REG_SIZE  1024
#define CANT_FILE 1000000
#define DIR_SIZE 256
#define CNN 128

struct date
{
	int tm_year;
	int tm_mon;
	int tm_mday;
	int tm_hour;
	int tm_min;
	int tm_sec;
};

//metodos
int create_server(int);   //crear el servidor
void request_handler(int);
int puthtml(char*,int n);
void del_client(int);
int decode(const char *, char *);
int ishex(int );
int get_filetype(char *,char *);
void sort(struct dirent *, int);
int sortbynameAsc(const void* ,const void*);
int sortbynameDesc(const void* , const void*);
int sortbysizeAsc(const void* ,const void*);
int sortbysizeDesc(const void* ,const void*);
int sortbydateAsc(const void* ,const void*);
int sortbydateDesc(const void* ,const void*);
int sorttm(struct date, struct date);
char* tipo(struct stat, struct dirent);
struct date trans(struct tm*);
void Foo(long int ,char * );

//variables globales
char* root;
int max_fd;
int fd_client[CNN];
int dfd_client[CNN];
off_t off_set[CNN];
int cant_client;
int listenfd;
char buffer[BUF_SIZE];
struct sockaddr_in clientaddr;
int addlen = sizeof(clientaddr);
fd_set fd_read;
fd_set fd_write;

char method[REG_SIZE];
char uri[REG_SIZE];

// para saber x donde ordenar
char sortby;
int sortbyname; 
int sortbysize;
int sortbydate;

int main(int argc, char const *argv[])
{
	signal(SIGPIPE,SIG_IGN);
	if(argc < 3)
	{
		printf("\"Insuficientes parametros\"\n");
		exit(0);
	}

	if(argc > 3)
	{
		printf("\"Parametros de mas\"\n");
		exit(0);
	}

	cant_client = 0;
	int i =0;
	for (int i = 0; i < CNN; ++i)
		fd_client[i] = 0;

	int port = atoi(argv[1]);
	root = argv[2];


	int fd_server = create_server(port);

	while(1)
	{
		//limpio el cjto de fd
		FD_ZERO(&fd_read);
        	FD_ZERO(&fd_write);

		//adiciono el fd del server
		FD_SET(listenfd, &fd_read);
		max_fd = listenfd;

		int i = 0;
		for (i = 0; i < CNN; ++i)
		{
			if(fd_client[i] > 0)
			{	
				if(!dfd_client[i])
					FD_SET(fd_client[i], &fd_read);
			    	FD_SET(fd_client[i], &fd_write);

				if(fd_client[i] > max_fd)
					max_fd = fd_client[i];
			}
		}

		int ret = select(max_fd + 1, &fd_read, &fd_write, NULL, NULL);

		if((ret < 0) && (errno != EINTR))
			printf("Select error.\n");

		if(FD_ISSET(listenfd, &fd_read))
		{   
		 	int new_socket;

            		if ((new_socket = accept(listenfd, (struct sockaddr *)&clientaddr, &addlen)) < 0)   
            		{   
                		perror("accept");   
                		exit(EXIT_FAILURE);   
            		}   	
                 
            		puts("Welcome message sent successfully");  
            		//add new socket to array of sockets 
            		i = 0; 
            		for (i = 0; i < CNN; i++)   
            		{   
                		//if position is empty  
                		if( fd_client[i] == 0 )   
                		{   
                    			fd_client[i] = new_socket;
                    			++cant_client;   
                   		 	break;   
                		}   	
            		}   
        	} 
        	i = 0;
        	for (i = 0; i < CNN; ++i)
        		request_handler(i);
	}

	return 0;
}

int create_server(int port)
 {
	struct sockaddr_in server;

	if((listenfd = socket(AF_INET, SOCK_STREAM, 0) ) < 0)
		return -1;

	int optval = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval , sizeof(int));

	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = INADDR_ANY;

	if(bind(listenfd, (struct sockaddr*) &server, sizeof(server)) < 0)
		return -1;

	if(listen(listenfd, CNN) < 0)
		return -1;

	return listenfd;
 }

void request_handler(int client)
{
	int sd = fd_client[client];

	if(FD_ISSET(sd, &fd_read))
    	{   
		char* dir = malloc(DIR_SIZE);
		char* temp=malloc(DIR_SIZE);
    	
    		strcpy(dir, root);
		memset(buffer,'\0',BUF_SIZE);
    	
    		int valread = read( sd , buffer, BUF_SIZE);
    		sscanf(buffer, "%s %s", method, uri);
		
		memset(buffer, '\0', BUF_SIZE);
		strcpy(buffer,uri);
		
		memset(uri,'\0',REG_SIZE);
		decode(buffer,uri);
		
		char* order;
		if((order= strstr(uri, "~")) != NULL)
		{
			order[0] = '\0';
			++order;

			sortby = order[0];

			if(order[0] == 'n')
				sortbyname = atoi(++order); 
			else if(order[0] == 's')
				sortbysize = atoi(++order);
			else
				sortbydate = atoi(++order);
		}
		else
		{
			sortby = 'n';
			sortbyname = 0;
			sortbysize = 0;
			sortbydate = 0;
		}

     		strcat(dir, uri);
     		
		memset(buffer, '\0', BUF_SIZE);
		struct stat statbuf;
		stat(dir,&statbuf);
		
		if(S_ISDIR(statbuf.st_mode))
		{ 
			if(!puthtml(dir, sd))
			{
				close( sd );   
				fd_client[client] = 0;
				cant_client--;  
				free(dir);
				free(temp);
				return;
			}

			sprintf(temp, "HTTP/1.1 200 OK\r\n");
			sprintf(temp, "%sContent-type: text/html; charset=UTF-8\r\n", temp);
			sprintf(temp, "%sContent-length: %li\r\n\r\n", temp, strlen(buffer));
			write(sd, temp, strlen(temp));
			write(sd, buffer, strlen(buffer));
			del_client(client);
		}
				
		else if (S_ISREG(statbuf.st_mode))
		{
	        if(!get_filetype(dir,method))
			{
				sprintf(temp,"HTTP/1.1 200 OK\n");
				sprintf(temp,"%sContent-Disposition: attachment\n",temp);
		        sprintf(temp,"%sContent-Length: %li\n\n",temp,statbuf.st_size);
			}
		else
			{
			    sprintf(temp, "HTTP/1.1 200 OK\n");
				sprintf(temp, "%sContent-type: %s\n", temp, method);
				sprintf(temp, "%sContent-Length: %li\n\n", temp,statbuf.st_size );
			}
			
			memset(buffer, '\0', BUF_SIZE);
			strcpy(buffer,temp);
			write(sd, buffer, strlen(buffer));
			int fd= open(dir,0);
		        dfd_client[client]=fd;
		}
		else
	    	{
			sprintf(buffer, "HTTP/1.1 404 Not Found\n");
	        	sprintf(buffer, "%sContent-Type: text/html; charset=UTF-8\n",buffer);
	        	sprintf(buffer ,"%sContent-Length: 28\n\n",buffer);
			sprintf(buffer, "%sERROR 404: File not found :(",buffer);  
			write(sd, buffer, strlen(buffer));
			del_client(client);
	    	}

		free(dir);
		free(temp);
  	}
	else if(FD_ISSET(sd,&fd_write)&&dfd_client[client])
	{
		if(sendfile(sd,dfd_client[client],&off_set[client],2000000)<=0)
			del_client(client);
	}  
}

int puthtml(char* dir,int sd)
{
	char* time;
	long int size=0;
	DIR * odir;
	
	if((odir=opendir(dir))==NULL)
	   	return 0;

	char * n= malloc(REG_SIZE);
	char *k=malloc(REG_SIZE);
	struct dirent *rodir;
	struct dirent *arrayrodir = malloc(sizeof(struct dirent) * CANT_FILE);
	sprintf(buffer, "<html><head><title>%s</title><meta charset=\"UTF-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\"></head>\r\n",dir);
	sprintf(buffer,"%s<body><CENTER>en: %s  ,<style> table {width: auto;border-collapse: collapse;border-spacing: 0;width: 100%;border: 1px solid #ddd;}th, td {width: auto;text-align: center;padding: 10px; font-size: 16px;}th { color: white;background: #4800ff;}tr:nth-child(even) {background-color: #f2f2f2}table tr.header, table tr:hover {background-color: #aaafff;}</style> <br><table CELLSPACING=\"10\" BORDER=\"1\" >", buffer, dir);
	if(uri[strlen(uri)-1]=='/')
		uri[strlen(uri)-1]='\0';

	int ind = 0; 
	while((rodir=readdir(odir))!=NULL)
			arrayrodir[ind++] = *rodir;

	sort(arrayrodir, ind);
	sprintf(buffer,"%s<tr><th>Tipo</th><th><a href=\"%s~n%d\">Nombre</a></th><th><a href=\"%s~s%d\">Tamaño</a></th><th><a href=\"%s~d%d\">Fecha</a></th></tr>\n",buffer, uri, sortbyname, uri, sortbysize, uri, sortbydate);
	
	int ind2 = 0;
	while(ind2 != ind)
	{
		rodir = &arrayrodir[ind2];
    	
    		strcpy(n,	uri);
		strcat(n,"/");
		strcat(n,rodir->d_name);
		strcpy(k,root);
		strcat(k,n);
		
		struct stat statbuf;
		stat(k,&statbuf);
        time=ctime(&statbuf.st_mtime);
		size=statbuf.st_size;
		Foo(size,k);
		
		char* m = malloc(256 * sizeof(char));
		strcpy(m, tipo(statbuf, *rodir));
		sprintf(buffer,"%s<tr><td>%s</td><td><a href=\"%s\">%s</a></td><td>%s</td><td>%s</td></tr>\n", buffer, m, n, rodir->d_name, k,time);
		
		++ind2;
		free(m);
	}

	sprintf(buffer,"%s</table></CENTER></body></html>\n", buffer);
	free(n);
	free(k);
	free(arrayrodir);
	return 1;
}
void del_client(int client)
{
	close(fd_client[client]);
	if(dfd_client[client])
		close( dfd_client[client] );   
    	fd_client[client] = 0;
	dfd_client[client]=0;
    	cant_client--;  
	off_set[client]=0;
}
int decode(const char *s, char *dec){
    char *o;
    const char *end = s + strlen(s);
    int c;
 
    for (o = dec; s <= end; o++) {
        c = *s++;
        if (c == '%' && (  !ishex(*s++)    ||
           !ishex(*s++)    ||
           !sscanf(s - 2, "%2x", &c)))
        return -1;
 
        if (dec) *o = c;
    }
 
    return o - dec;
}
int ishex(int x){
    return  (x >= '0' && x <= '9')  ||
        (x >= 'a' && x <= 'f')  ||
        (x >= 'A' && x <= 'F');
}
int get_filetype(char *filename,char *filetype)
{
    if(strstr(filename,".htm"))
        {strcpy(filetype,"text/html"); return 1;}
    else if(strstr(filename,".gif"))
        {strcpy(filetype,"image/gif");return 1;}
    else if(strstr(filename,".jpg"))
       { strcpy(filetype,"image/jpeg");return 1;}
    else if(strstr(filename,".avi"))
	    { strcpy(filetype,"audio/avi");return 1;}
	else if(strstr(filename,".mp4"))
	    { strcpy(filetype,"video/mp4");return 1;}
	else if(strstr(filename,".c"))
	    { strcpy(filetype,"text/plain");return 1;}
	else if(strstr(filename,".pdf"))
	    { strcpy(filetype,"application/pdf");return 1;}
		
    return 0;	
}

char* tipo(struct stat statt, struct dirent rodir)
{
	if(S_ISDIR(statt.st_mode))
		return "dir";
	else
	{
		char* k;
		char* t;
		t = rodir.d_name;
		int cant = 0;

		while( (t=strstr(t, ".")) != NULL)
		{
			++t;
			k = t;
			++cant;
		}

		if(!cant)
			return " ";
		return k;
	}
}

//sort
void sort(struct dirent * arrayrodir, int ind)
 {
	if(sortby == 'n')
	{
		if(!sortbyname)
			qsort(arrayrodir, ind, sizeof(struct dirent), sortbynameAsc);
		else
			qsort(arrayrodir, ind, sizeof(struct dirent), sortbynameDesc);
		sortbyname = (sortbyname + 1) % 2;
	}
	else if(sortby == 's')
	{
		if(!sortbysize)
			qsort(arrayrodir, ind, sizeof(struct dirent), sortbysizeAsc);
		else
			qsort(arrayrodir, ind, sizeof(struct dirent), sortbysizeDesc);
		sortbysize = (sortbysize + 1) % 2;
	}
	else
	{
		if(!sortbydate)
			qsort(arrayrodir, ind, sizeof(struct dirent), sortbydateAsc);
		else
			qsort(arrayrodir, ind, sizeof(struct dirent), sortbydateDesc);
		sortbydate = (sortbydate + 1) % 2;
	}	
 }
int sortbynameAsc(const void* arg1, const void* arg2)
 {
	struct dirent odir1 = *((struct dirent const*)arg1);
	struct dirent odir2 = *((struct dirent const*)arg2);

	return strcmp(odir1.d_name, odir2.d_name);
 }
int sortbynameDesc(const void* arg1, const void* arg2)
 {
	return -1 * sortbynameAsc(arg1, arg2);
 }
int sortbysizeAsc(const void* arg1, const void* arg2)
 {
	struct dirent odir1 = *((struct dirent const*)arg1);
	struct dirent odir2 = *((struct dirent const*)arg2);

	char* dir1 = malloc(REG_SIZE);
	char* dir2 = malloc(REG_SIZE);
	strcpy(dir1, root);
	strcat(dir1, uri);
	strcat(dir1, "/");   
	strcpy(dir2, dir1);
	strcat(dir1, odir1.d_name);
	strcat(dir2, odir2.d_name);

	struct stat stat1;
	stat(dir1, &stat1);
	long int size1 = stat1.st_size;

	struct stat stat2;
	stat(dir2, &stat2);
	long int size2 = stat2.st_size;

	free(dir1);
	free(dir2);
	size1= (size1 - size2);
	if(size1>0)return 1;
	if(size1<0)return -1;
	return size1;
 }
int sortbysizeDesc(const void* arg1, const void* arg2)
 {
	return -1 * sortbysizeAsc(arg1, arg2);
 }
int sortbydateAsc(const void* arg1, const void* arg2)
 {
	struct dirent odir1 = *((struct dirent const*)arg1);
	struct dirent odir2 = *((struct dirent const*)arg2);

	char* dir1 = malloc(REG_SIZE);
	char* dir2 = malloc(REG_SIZE);
	strcpy(dir1, root);
	strcat(dir1, uri);
	strcat(dir1, "/");   
	strcpy(dir2, dir1);
	strcat(dir1, odir1.d_name);
	strcat(dir2, odir2.d_name);

	struct stat statt;
	stat(dir1, &statt);
	struct tm* tmm = localtime(&statt.st_mtime);
	struct date date1 = trans(tmm);
	stat(dir2, &statt);
	tmm = localtime(&statt.st_mtime);
	struct date date2 = trans(tmm);

	free(dir1);
	free(dir2);
	return sorttm(date1, date2);
 }
int sortbydateDesc(const void* arg1, const void* arg2)
 {
	return -1 * sortbydateAsc(arg1, arg2);
 }
int sorttm(struct date tm1, struct date tm2)
 {	
	if(tm1.tm_year != tm2.tm_year)
		return tm1.tm_year - tm2.tm_year;
	if(tm1.tm_mon != tm2.tm_mon)
		return tm1.tm_mon - tm2.tm_mon;
	if(tm1.tm_mday != tm2.tm_mday)
		return tm1.tm_mday - tm2.tm_mday;
	if(tm1.tm_hour != tm2.tm_hour)
		return tm1.tm_hour - tm2.tm_hour;
	if(tm1.tm_min != tm2.tm_min)
		return tm1.tm_min - tm2.tm_min;
	if(tm1.tm_sec != tm2.tm_sec)
		return tm1.tm_sec - tm2.tm_sec;
	return 0;
 }

struct date trans(struct tm* tmm)
 {
	struct date ddate;
	ddate.tm_year = tmm->tm_year;
	ddate.tm_mon = tmm->tm_mon;
	ddate.tm_mday = tmm->tm_mday;
	ddate.tm_hour = tmm->tm_hour;
	ddate.tm_min = tmm->tm_min;
	ddate.tm_sec = tmm->tm_sec;
	return ddate;
 }

char *map[]={"B","KB","MB","GB","TB"};
void Foo(long int x,char * n)
 {
	float f=x;
	int times=0;
	double pow=1;

	while((f/=1000)>=1)
	{ 
		times++;
	   	pow*=1000;		
    }

	float a=x/pow;
  	sprintf(n,"%f %s",a,map[times]);
 }
