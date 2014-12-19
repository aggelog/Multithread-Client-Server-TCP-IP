/********************************/
/* Name : Aggelogiannis Giannis */
/* A.M  : 2556		        */	
/* mail : aggelog@csd.uoc.gr    */
/********************************/
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/times.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <time.h>

#define MAX_LENGTH 255
#define TRUE 1
#define FALSE 0
#define PORT 5759
#define BACKLOG 20
#define SHM_SIZE 1000

/* key and id of shared memory segment */
int mem_key, shmid;
int *queue = NULL;
int queue_size = 0;
int last_in_room;
int exit_t = 0;
int qu = 0;
int given = 0;
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

void *get_in_addr(struct sockaddr *sa);
void sig_handler(int signo);
int create_shared_memory(void);
int deallocate_shared_memory(void);
void *room_check(void *arg);
void next_department(void);

int main(int argc, char **argv)
{
	pthread_t t1;
	int sock, numbytes, newsock, i, timecnt;
	unsigned int server_l, client_l;                 /* Length of server and client addr data structure */
	char buffer[MAX_LENGTH];
	char sent_buffer[5];
	char s[INET_ADDRSTRLEN];
	struct sockaddr_in server_addr, client_addr;    /* Info about server and client addr */


	if( (signal(SIGINT,sig_handler)) == SIG_ERR)
	{
		perror("\nsignal() failed!\n");
		fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
		exit(EXIT_FAILURE);
	}

	if( (signal(SIGTERM,sig_handler)) == SIG_ERR)
	{
		perror("\nsignal() failed!\n");
		fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
		exit(EXIT_FAILURE);
	}


	/* initialize random seed */
	srand(time(NULL));

	/* Creating socket for incoming connextions */
	if((sock = socket(AF_INET,SOCK_STREAM,0)) < 0)
	{
		perror("\nsocket() error.\n");
		fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
		exit(EXIT_FAILURE);
	}
	else
	{
		fprintf(stdout,"\nServer: socket is ready ..\n");
		sleep(2);
	}

	memset(&server_addr,0,sizeof(server_addr));	 /* Initiate all values of the struct to 0 */
	server_addr.sin_family = AF_INET;		 /* Connection-oriented AF_INET socket / TCP */
	server_addr.sin_addr.s_addr = htons(INADDR_ANY); /* Special IP / INADDR_ANY server working without knowing the ip addr of the machine*/
	server_addr.sin_port = htons(PORT);		 /* or, in the case of a machine with multiple network interfaces */
							 /* it allowed your server to receive packets destined to any of the interfaces*/
	server_l = sizeof(server_addr);

	/* Bind to the local addr */
	if(bind(sock,(struct sockaddr *) &server_addr,server_l) < 0 )
	{
		close(sock);
		perror("\nServer: failed to bind\n");
		fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
		exit(EXIT_FAILURE);
	}
	else
	{
		fprintf(stdout,"\nServer: bind is ok ..\n");
		sleep(2);
	}

	/* Mark the socket so it will listen for incoming connections */
	if(listen(sock,BACKLOG) < 0)
	{
		close(sock);
		perror("\nlisten() error.\n");
		fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
		exit(EXIT_FAILURE);
	}
	else
	{
		fprintf(stdout,"\nServer: waiting for connections ...\n\n");
		sleep(2);
	}

	if( (pthread_create(&t1,NULL,room_check,&timecnt)) != 0)
	{
		fprintf(stderr,"SERVER_ERROR: cant create the thread.\n");
		exit(EXIT_FAILURE);
	}

	while(TRUE)
	{
		client_l = sizeof(client_addr);

		/* Wait for a client to connect */
		if( (newsock = accept(sock, (struct sockaddr *) &client_addr, &client_l)) < 0)
		{
			close(sock);
			perror("\naccept() error.\n");
			fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
			continue;
		}

		if( (inet_ntop(client_addr.sin_family,get_in_addr((struct sockaddr *)&client_addr),s,sizeof(s))) == NULL)
		{
			close(newsock);
			close(sock);
			perror("\ninet_ntop() error.\n");
			fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
			if(given == 1)deallocate_shared_memory();
			exit(EXIT_FAILURE);
		}
		/* newsock is now connected to a client */
		fprintf(stdout,"\nServer: got connection from %s\n",s);

		do
		{
			/* recieving bytes from client */
			numbytes = recv(newsock,buffer,MAX_LENGTH-1,0);
			if(numbytes > 0 )
			{
				buffer[numbytes-1] = '\0';
				
				/* check if the client requests a room */
				if( ((strcmp("GETM",buffer)) == 0) || ((strcmp("GETC",buffer)) == 0) )
				{
					fprintf(stdout,"\nServer: client requests a room key!\n");
					/* check if there is already a room available */
					if(given != 0)
					{
						/* Send the key to the client */
						sprintf(sent_buffer,"%d",mem_key);
						if((send(newsock,sent_buffer,strlen(sent_buffer),0)) == -1)
						{
							perror("\nsend() error.\n");
							fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
							break;
						}
						/* insert the client to the queue */
						if((strcmp("GETM",buffer)) == 0)
						{
							if((queue = realloc(queue,(queue_size + 1) * sizeof(int))) == NULL)
							{
								perror("\nrealloc() error.\n");
								fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
							}
							queue[queue_size] = 2;
							queue_size++;
						}
						else
						{
							if((queue = realloc(queue,(queue_size + 1) * sizeof(int))) == NULL)
							{
								perror("\nrealloc() error.\n");
								fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
							}
							queue[queue_size] = 1;
							queue_size++;
						}
					}
					else
					{
						fprintf(stdout,"\nServer : Generating key ....\n");
						sleep(1);
						/* generate a random number between 1 and 1000 */
						mem_key = rand() % 1000 + 1;
						fprintf(stdout,"\nServer : key = %d\n",mem_key);
						if(create_shared_memory() == 0)
						{
							fprintf(stdout,"\nServer: Cant create/attach Shared Memory.\n");
							fprintf(stdout,"\nServer: Connection closed.\n");
							break;
								
						}
						fprintf(stdout,"\nServer : Sending the key ...\n");
						/* Send the key to the client */
						sprintf(sent_buffer,"%d",mem_key);
						if((send(newsock,sent_buffer,strlen(sent_buffer),0)) == -1)
						{
							perror("\nsend() error.\n");
							fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
							break;
						}
						/* insert the client to the queue */
						if((strcmp("GETM",buffer)) == 0)
						{
							if((queue = realloc(queue,(queue_size + 1) * sizeof(int))) == NULL)
							{
								perror("\nrealloc() error.\n");
								fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
							}
							queue[queue_size] = 2;
							queue_size++;
						}
						else
						{
							if((queue = realloc(queue,(queue_size + 1) * sizeof(int))) == NULL)
							{
								perror("\nrealloc() error.\n");
								fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
							}
							queue[queue_size] = 1;
							queue_size++;
						}
						given = 1;
					}
				}
				else
				{
					fprintf(stdout,"\nServer: received mssg: %s\n",buffer);
				}

				for(i=0;i<MAX_LENGTH;i++)
				{
					buffer[i] = '\0';
				}
			}
			else if(numbytes == 0)
			{
				fprintf(stdout,"\nServer: Connection closed.\n");
			}
			else
			{
				close(newsock);
				close(sock);
				perror("\nrecv() error.\n ");
				fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
				if(given == 1)deallocate_shared_memory();
				exit(EXIT_FAILURE);
			}

		}while (numbytes > 0);

		close(newsock);

		if(queue != NULL)
		{
			fprintf(stdout,"\nqueue :");
			for(i = 0; i < queue_size; i++)
			{
				fprintf(stdout," %d ",queue[i]);
			}
		}
		fprintf(stdout,"\n");
	}

	return 0;
}

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


void sig_handler(int signo)
{
	if(signo == SIGINT)
	{
		exit_t = 1;
		fprintf(stdout,"\nSERVER: received SIGINT\n");
		if(given == 1)
		{
			deallocate_shared_memory();
			if( (sem_destroy(&shm_ptr->csd_mutex)) != 0)
			{
				perror("\nsem_destroy() error\n");
				fprintf(stderr,"\nSERVER_ERROR: errno = %d.\n", errno);
				exit(EXIT_FAILURE);	
			}

			if( (sem_destroy(&shm_ptr->math_mutex)) != 0)
			{
				perror("\nsem_destroy() error\n");
				fprintf(stderr,"\nSERVER_ERROR: errno = %d.\n", errno);
				exit(EXIT_FAILURE);	
			}
		}
		exit(EXIT_SUCCESS);
	}

	if(signo == SIGTERM)
	{
		exit_t = 1;
		fprintf(stdout,"\nSERVER: received SIGTERM\n");
		if(given == 1)
		{
			deallocate_shared_memory();
			if( (sem_destroy(&shm_ptr->csd_mutex)) != 0)
			{
				perror("\nsem_destroy() error\n");
				fprintf(stderr,"\nSERVER_ERROR: errno = %d.\n", errno);
				exit(EXIT_FAILURE);	
			}

			if( (sem_destroy(&shm_ptr->math_mutex)) != 0)
			{
				perror("\nsem_destroy() error\n");
				fprintf(stderr,"\nSERVER_ERROR: errno = %d.\n", errno);
				exit(EXIT_FAILURE);	
			}
		}
		exit(EXIT_SUCCESS);
	}
}

int deallocate_shared_memory(void)
{
	fprintf(stdout,"\nSERVER: deallocating shared memory\n");
	sleep(1);
	if((shmctl(shmid, IPC_RMID, NULL)) == 0)
	{
		return 1;
	}
	else
	{
		perror("\nshmctl() error.\n ");
		fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
		return 0;
	}	
}

int create_shared_memory(void)
{
	while( (shmid = shmget(mem_key,SHM_SIZE,IPC_CREAT | 0666)) == -1)
	{
		mem_key = rand() % 1000 + 1;
		perror("\nshmget() error.\n ");
		fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
		qu++;
		fprintf(stderr,"SERVER_ERROR: trying to allocate shared memory with different key...(%d)\n",qu);
		fprintf(stdout,"\nServer :Generating key ....\n");
		sleep(1);
		if(qu > 4)
		{
			fprintf(stderr,"SERVER_ERROR: cant allocate shared memory!\n");
			return 0;
		}
	}
	qu = 0;
	if( (shm_ptr = (struct shm_data *)shmat(shmid,(void *)0, 0)) == (struct shm_data *)(-1) )
	{
		perror("\nshmat() error.\n ");
		fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
		deallocate_shared_memory();
		return 0;
	}

	shm_ptr->student_counter = 0;
	fprintf(stdout,"shm_ptr->student_counter = %d\n",shm_ptr->student_counter);
	
	/* Initialize servers semaphores to be locked */

	if( (sem_init(&shm_ptr->csd_mutex, 1, 0)) != 0)
	{
		perror("\nsem_init() error\n");
		fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
		return 0;
	}

	if( (sem_init(&shm_ptr->math_mutex, 1, 0)) != 0)
	{
		perror("\nsem_init() error\n");
		fprintf(stderr,"SERVER_ERROR: errno = %d.\n", errno);
		return 0;
	}

	return 1;
}


void *room_check(void *arg)
{

	struct tms cpu_time;
	clock_t start,end,elapsed;
	int count = 0, i;
	
	if((start = times(&cpu_time)) < 0)
	{
		fprintf(stderr,"times() failed!\n");
		pthread_exit(0);
	}

	while(TRUE)
	{
		if((shm_ptr != NULL) && (queue_size > 0))
		{
			if(shm_ptr->occupied == 0)
			{
				shm_ptr->occupied = 1;
				next_department();
			}
		}

		if(exit_t == 1)
		{
			pthread_exit(0);
		}
		if((end = times(&cpu_time)) < 0)
		{
			fprintf(stderr,"times() failed!\n");
			pthread_exit(0);
		} 
		elapsed = end - start;

		if( ((double)elapsed/(double)sysconf(_SC_CLK_TCK)) == 1.0)
		{
			count ++;
			if(shm_ptr == NULL)
			{
				fprintf(stdout,"Server:%d)there are no students in the Room.\n",count);
			}
			else
			{
				if((shm_ptr->student_counter) == 0)
				{
					fprintf(stdout,"Server:%d)there are no students in the Room.\n",count);
					if((start = times(&cpu_time)) < 0)
					{
						fprintf(stderr,"times() failed!\n");
						pthread_exit(0);
					}
					continue;
				}
				fprintf(stdout,"Server:%d) ",count);
				for(i = 0; i < shm_ptr->student_counter; i++)
				{
					if(last_in_room == 1)fprintf(stdout,"C ");
					if(last_in_room == 2)fprintf(stdout,"M ");
				}
				fprintf(stdout,"(%d).\n",shm_ptr->student_counter);
			}
			if((start = times(&cpu_time)) < 0)
			{
				fprintf(stderr,"times() failed!\n");
				pthread_exit(0);
			}
		}
	}
}

void next_department()
{
	int i = 0, z = 0, j = 0;
	int *tmp_queue = NULL;

	if(queue_size == 0)
	{
		return; /* none is next */
	}
	else if(queue_size == 1)
	{
		if(queue[0] == 1) /* One stands for CSD students */
		{	
			last_in_room = 1;
			/*free(queue);*/
			queue = NULL;
			queue_size = 0;
			sem_post(&shm_ptr->csd_mutex);
			return;
		}
		else
		{
			last_in_room = 2;
			/*free(queue);*/
			queue = NULL;
			queue_size = 0;
			sem_post(&shm_ptr->math_mutex);
			return;
		}
	}
	else
	{
		if(last_in_room == 1)
		{
			for(i = 0; i < queue_size; i++)
			{
				if(queue[i] == 2) /* Two stands for MATH students */
				{
					last_in_room = 2;
					tmp_queue = malloc((queue_size - 1)*sizeof(int));
					for(j = 0; j < (queue_size - 1); j++)
					{
						if(j == i)z++;
						tmp_queue[j] = queue[z];
						z++;
					}
					queue_size--;
					queue = tmp_queue;
					/*free(tmp_queue);*/
					tmp_queue = NULL;
					sem_post(&shm_ptr->math_mutex);
					return;
				}
			}
			tmp_queue = malloc((queue_size - 1)*sizeof(int));
			for(j = 0; j < (queue_size - 1); j++)
			{
				if(j == 0)z++;
				tmp_queue[j] = queue[z];
				z++;
			}
			queue_size--;
			queue = tmp_queue;
			/* free(tmp_queue); */
			tmp_queue = NULL;
			sem_post(&shm_ptr->csd_mutex);
			return;
		}
		else
		{
			for(i = 0; i < queue_size; i++)
			{
				if(queue[i] == 1)
				{
					last_in_room = 1;
					tmp_queue = malloc((queue_size - 1)*sizeof(int));
					for(j = 0; j < (queue_size - 1); j++)
					{
						if(j == i)z++;
						tmp_queue[j] = queue[z];
						z++;
					}
					queue_size--;
					queue = tmp_queue;
					/*free(tmp_queue);*/
					tmp_queue = NULL;
					sem_post(&shm_ptr->csd_mutex);
					return;
				}
			}
			tmp_queue = malloc((queue_size - 1)*sizeof(int));
			for(j = 0; j < (queue_size - 1); j++)
			{
				if(j == 0)z++;
				tmp_queue[j] = queue[z];
				z++;
			}
			queue_size--;
			queue = tmp_queue;
			/*free(tmp_queue);*/
			tmp_queue = NULL;
			sem_post(&shm_ptr->math_mutex);
			return;
		}
	}
}

