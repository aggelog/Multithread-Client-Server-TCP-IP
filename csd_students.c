/********************************/
/* Name : Aggelogiannis Giannis */
/* A.M  : 2556		        */	
/* mail : aggelog@csd.uoc.gr    */
/********************************/
#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <strings.h>

#define MAX_LENGTH 255
#define TRUE 1
#define FALSE 0
#define PORT 5759
#define SHM_SIZE 1000

/* key and id of shared memory segment */
int mem_key, shmid;
/* shared memorys data */
struct shm_data
{
	int student_counter;
	sem_t csd_mutex;
	sem_t math_mutex;
	int occupied;
};
/* pointer to the shared memory segment */
struct shm_data *shm_ptr;
/* semaphore declaration */
sem_t mutexEnter, mutexLeave;

/* functions declaration */
int attach_shared_memory(void);
void *do_meeting(void *);

int main(int argc, char **argv)
{
	pthread_t *Thread;
	int sock, numbytes, Rnumbytes, i, j;
	int students;
	int *id;
	char buffer[MAX_LENGTH];
	char recieved_buffer[5];
	struct sockaddr_in server_addr; /* Info of server adrr */

	if(argc < 2)
	{
		fprintf(stderr,"\nCSD_ERROR: You must give the IP in order to connect.\n");
		exit(EXIT_FAILURE);
	}

	if(argc < 3)
	{
		fprintf(stderr,"\nCSD_ERROR: You must give a number of students.\n");
		exit(EXIT_FAILURE);
	}

	if(argc < 4)
	{
		fprintf(stderr,"\nCSD_ERROR: Zero messages have been entered.\n");
		exit(EXIT_FAILURE);
	}

	/* Creating a stream socket*/
	if((sock = socket(AF_INET,SOCK_STREAM,0)) == -1)
	{
		perror("\nsocket() error.\n");
		fprintf(stderr,"CSD_ERROR: errno = %d.\n", errno);
		exit(EXIT_FAILURE);
	}
	else
	{
		fprintf(stdout,"\nCSD_Client: socket is ready ..\n");
		sleep(2);
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT);
	server_addr.sin_addr.s_addr  = inet_addr(argv[1]);

	/* Zero out the rest of the structure */
	memset(&(server_addr.sin_zero),'\0',8);

	/* Establish the connection to the server*/
	if(connect(sock,(struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1)
	{
		perror("\nconnect() error.\n");
		fprintf(stderr,"CSD_ERROR: errno = %d.\n", errno);
		exit(EXIT_FAILURE);
	}
	else
	{
		fprintf(stdout,"\nCSD_Client: Connextion Established!\n");
		sleep(2);
	}

	for(j=3;j<argc;j++)
	{

		for(i=0;i<MAX_LENGTH;i++)
		{
			buffer[i]='\0';
		}

		strcpy(buffer,argv[j]);
		buffer[strlen(argv[j])]=' ';

		/* Sending bytes to server */
		if((numbytes = send(sock,buffer,strlen(buffer),0)) == -1)
		{
			perror("\nsend() error.\n");
			fprintf(stderr,"CSD_ERROR: errno = %d.\n", errno);
			exit(EXIT_FAILURE);
		}
		buffer[strlen(argv[j])]='\0';
		if(strcmp("GETC",buffer) == 0)
		{
			/* recieving bytes from server */
			Rnumbytes = recv(sock,recieved_buffer,4,0);
			if(Rnumbytes > 0 )
			{
				recieved_buffer[Rnumbytes] = '\0';
				mem_key = atoi(recieved_buffer);
				fprintf(stdout,"\nCSD: recieved the key :%d\n",mem_key);
				if(attach_shared_memory() == 0)
				{
					close(sock);
					fprintf(stderr,"CSD_ERROR:cant attach shared memory\n");
					exit(EXIT_FAILURE);
				}
				else
				{
					fprintf(stdout,"\nCSD: shared memory attached!\n");
					students = atoi(argv[2]);
					fprintf(stdout,"\nCSD: The total students are : %d\n",students);
				}
			}
			else if(numbytes == 0)
			{
				fprintf(stdout,"\nCSD: recieved 0 bytes from Server.\n");
			}
			else
			{
				close(sock);
				perror("\nrecv() error.\n ");
				fprintf(stderr,"CSD_ERROR: errno = %d.\n", errno);
				exit(EXIT_FAILURE);
			}
		}

		/* giving some time to server to receive the message before client send the next one*/
		usleep(10000);

	}

	close(sock);

	/* initialize random seed */
	srand(time(NULL));

	if( (sem_init(&mutexEnter, 0, 1)) != 0)
	{
		perror("\nsem_init() error\n");
		fprintf(stderr,"CSD_ERROR: errno = %d.\n", errno);
		exit(EXIT_FAILURE);
	}

	if( (sem_init(&mutexLeave, 0, 1)) != 0)
	{
		perror("\nsem_init() error\n");
		fprintf(stderr,"CSD_ERROR: errno = %d.\n", errno);
		exit(EXIT_FAILURE);
	}

	Thread = (pthread_t *)malloc(students * sizeof(pthread_t));

	id = (int *)malloc(students * sizeof(int));
	for(i = 0; i < students; i++)
	{
		id[i] = i;
	}
	/* lock to start changing the values of shm counter                       */
	sem_wait(&shm_ptr->csd_mutex); 	                                  
	                                             
	for(i = 0; i < students; i++)
	{
		if( (pthread_create(&(Thread[i]), NULL,do_meeting,&(id[i]))) != 0)
		{
			perror("\npthread_create() error\n");
			fprintf(stderr,"CSD_ERROR: errno = %d.\n", errno);
			exit(EXIT_FAILURE);
		}
	}

	for(i = 0; i < students; i++)
	{
		if( (pthread_join(Thread[i], NULL)) != 0)
		{
			perror("\npthread_join() error\n");
			fprintf(stderr,"CSD_ERROR: errno = %d.\n", errno);
			exit(EXIT_FAILURE);
		}
	}

	/* report server that you finished changing the values of shm counter */
	shm_ptr->occupied = 0;					      

	if( (sem_destroy(&mutexEnter)) != 0)
	{
		perror("\nsem_destroy() error\n");
		fprintf(stderr,"CSD_ERROR: errno = %d.\n", errno);
		exit(EXIT_FAILURE);	
	}

	if( (sem_destroy(&mutexLeave)) != 0)
	{
		perror("\nsem_destroy() error\n");
		fprintf(stderr,"CSD_ERROR: errno = %d.\n", errno);
		exit(EXIT_FAILURE);	
	}

	return 0;
}

void *do_meeting(void *student)
{
	int x = *((int *) student);
	int A,B;

	A = rand() % 10 + 1;
	B = rand() % 10 + 1;

	printf("Thread %d: Waiting %d to enter the Room...\n", x, A);
	sleep(A);
    	sem_wait(&mutexEnter);       /* down semaphore */
    	printf("Thread %d: entered the Room...\n", x);
    	shm_ptr->student_counter++;    
   	sem_post(&mutexEnter);       /* up semaphore */
	
	printf("Thread %d: Waiting %d to leave the Room...\n", x, B);
	sleep(B);
	sem_wait(&mutexLeave);
	printf("Thread %d: left the Room...\n", x);
	shm_ptr->student_counter--;
	sem_post(&mutexLeave);

	pthread_exit(0);
}

int attach_shared_memory(void)
{

	if( (shmid = shmget(mem_key,SHM_SIZE,IPC_CREAT | 0666)) == -1)
	{
		perror("\nshmget() error.\n ");
		fprintf(stderr,"CSD_ERROR: errno = %d.\n", errno);
		return 0;
	}

	if( (shm_ptr = (struct shm_data *)shmat(shmid,(void *)0, 0)) == (struct shm_data *)(-1) )
	{
		perror("\nshmat() error.\n ");
		fprintf(stderr,"CSD_ERROR: errno = %d.\n", errno);
		return 0;
	}	

	return 1;
}

