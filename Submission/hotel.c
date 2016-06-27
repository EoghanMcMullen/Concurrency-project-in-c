#include <string.h>//gets rid of memcopy warnings
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <time.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#define ETIMEDOUT 110
//structure to hold a room number and a time
typedef struct wakeUps 
{
	int roomNum;
	time_t alarmTime;
} wakeUps;
//structure that will have a pointer to an array of
//wakeUps, along with data to handle it.
typedef struct infoArray
{
	wakeUps *info;
    	int entries; //how many entries we have inserted
    	int space; //shows how much space available
    	int mulSize;//used to increase array memory
	int expired;//keeps track of number of expired alarms
    
}infoArray;
//-------------------------------------------------------
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
//-------------------------------------------------------
//function to initialize the array
//-------------------------------------------------------
void init(infoArray *current)
{
 	//allocate space for 10 wakeups
	current->info = (wakeUps *)malloc(10 * sizeof(wakeUps));
    	current->entries = 0;
    	current->space = 10;
	//set mulSize to 2 => use to doble array size.
    	current->mulSize = 2;
	current->expired = 0;
    
    	//set all elements of array to 0
    	memset(&current->info[0], 0, sizeof(wakeUps) * current->space);
}

//-------------------------------------------------------
//Function to allocate the array more memory
//-------------------------------------------------------
void resize(infoArray *current)
{
    	//each time resize is called, double the array size
	//gives new portion of memory to current->info	
    	current->info = (wakeUps *)realloc(current->info, current->mulSize * sizeof(wakeUps) * 10);
	if (current->info == NULL)
	{
		printf("Could not allocate more memory!!!");
		exit(1);
	}
    	//multiply number of spaces by 2
    	current->space = current->space * 2;
	//doble mulSize so array is * 4 next time, * 8 etc...
    	current->mulSize *= 2;

    	// Initialize the last/new elements of the reallocated array
    	memset(&current->info[current->entries],0,sizeof(wakeUps) * (current->space - current->entries));
}

//-------------------------------------------------
//-----Method to swap parent with child in the heap
//-------------------------------------------------
//----use int to say where the child is 
void swapNodes(infoArray *current,int numEntries)
{
	//parent of i is at (i-1)/2
	int c = numEntries;
	int p = (c-1)/2;
	
	wakeUps child = current->info[c];
	wakeUps parent = current->info[p];

	current->info[p] = child;
	current->info[c] = parent;
}


//--------------------------------------------------
//inserts a new structure into array, keeps in order
//root is at info[0]
//--------------------------------------------------

void insert(infoArray *current,wakeUps newEntry)
{
	//have we run out of space?
    	if(current->entries == current->space)
    	{
		//resize the array if we have
    	    	resize(current);
	    	printf("resize called, number of spaces: %d \n\n", current->space);
    	}

    	int numEntries = current->entries;

    	//check if its the first entry
    	//if it is it becomes the root	
    	if(numEntries == 0)
    	{
        	current->info[numEntries].alarmTime = newEntry.alarmTime;
        	current->info[numEntries].roomNum = newEntry.roomNum;
    	}
    	else
    	{
		//place new wakeUp at end 
		current->info[numEntries] = newEntry;

		//rebuildTree;

		//while we have values left and the new alarm time < its parent
		while(numEntries >= 0 && current->info[numEntries].alarmTime < current->info[(numEntries-1)/2].alarmTime)
		{
			swapNodes(current,numEntries);
			numEntries = (numEntries-1)/2;
		}	

    	}
    
   	current->entries++;
}

//-------------------------------------------------
//Function to get the next value at first loc in the array
//-------------------------------------------------

wakeUps getNextTime(infoArray *current)
{	
    	wakeUps retVal;

    	time_t time = current->info[0].alarmTime;
    	int rNum = current->info[0].roomNum;

    	retVal.roomNum =rNum;
    	retVal.alarmTime = time;
    	return retVal;    
}
//----------------------------------------------
//function to remove an alarm time from the array
//----------------------------------------------
void removeElement(infoArray *current)
{
   	//------swap first and last elements-----------
   	//backup both then insert at appropriate loc
   	wakeUps first = current->info[0];
   	wakeUps last = current->info[current->entries - 1];

   	//last element goes at loc[0]
   	//first element goes to loc[entries - 1]	

   	current->info[0] = last;
   	current->info[current->entries - 1] = first;
   	//forget about the entry we want deleted
   	current->entries--;		

   	//now item at loc[0] might not be most important in the queue
   	//its either item at loc[1] or loc[2]
   	//compare these two items and then swap item at loc[0] for the smaller

   	int numEntries = current->entries;

 
   	int root = 0;
   	int left = 1;
   	int right = 2;
   	int moveHere;
	//run until right < numentries => checks entire array
   	while(right < numEntries)
   	{
		//check if the root is larger than either of its children
	   	if(current->info[root].alarmTime > current->info[left].alarmTime
	      	   ||current->info[root].alarmTime > current->info[right].alarmTime) 	
	   	{
			//check if left or right is smaller
	   		if(current->info[left].alarmTime <= current->info[right].alarmTime)
	   		{
				moveHere = left;
			}
			else moveHere = right;
	     
			//swap the appropriate value
			wakeUps p = current->info[root];
			wakeUps c = current->info[moveHere];

			current->info[root] = c;
			current->info[moveHere]= p;
	    	}
		//move on to next elements
		root = (2 * root) + 1;
	    	left = 2 * root + 1;
	    	right = 2 * root + 1;
    	}     
}
//--------------------------------------
//Thread will run this function.
//Generates the random times and room numbers.
//Adds them to the dynamic array
//-------------------------------------- 
static void* generate(void *alarmData)
{
	//have link point to our infoArray structure
	infoArray* data = (infoArray *) alarmData;
	
	//use these ints for generating times
	int s1,s2,s3;
	//change the random algorithm to use current time
	//Makes random every run
	srand(time(NULL));
	while(1)
	{
		 //wait for the lock
		 pthread_mutex_lock(&mutex);
		 //generate the random room numbers and alarm times
		 int roomNo = (rand()% (1-10000) +1);
		 int randSecs = (rand_r(&s3) % 100);
		 //current time + up to 100 seconds in the future
		 time_t wakeTime = time(NULL)+randSecs;

		 //add the information to the wakeupcall structure
		 wakeUps wake;
		 wake.roomNum = roomNo;
		 wake.alarmTime = wakeTime;

		 
		 //use insert fuction to keep in order of alarms to go off
		 insert(data, wake);
		 //if the time we just inserted is at the root, signal the waiter thread
		 if(wakeTime == data->info[0].alarmTime)
		 {
              		pthread_cond_signal(&cond);
                 }

		 //print the info registered
		 printf("Registering: \t%d \t%s \n", roomNo, ctime(&wakeTime));

		 //unlock and go to sleep => chance for other thread to work
		 pthread_mutex_unlock(&mutex);
		 //put generate to sleep for up to 5 seconds
		 sleep(rand_r(&s2) % 5);
	}

}
//-----------------------------------------------------
//This is responsible for listening for alarms to go off
//Prints appropriate values when alarm goes off
//run by a thread
//----------------------------------------------------
static void* writer(void * alarmData) 
{
	//access to alarmData
	infoArray* data = (infoArray *) alarmData;
	int error;
    	struct timespec time;
	
	while(1)
	{
		 //wait for mutex
		 pthread_mutex_lock(&mutex);
		 
		//while we have no entries wait
		 while(data->entries == 0)
		 {
			pthread_cond_wait(&cond,&mutex);
		 }
		 wakeUps wake =  getNextTime(data);

		 time.tv_sec = wake.alarmTime;
        	 time.tv_nsec = 0;

		 //will return an error if the time specified runs out 
        	 error = pthread_cond_timedwait(&cond,&mutex, &time);

		 //if error returns that the time ran out 
        	 if(error == ETIMEDOUT)
		 {
        	     printf("Wake up:\t%d %s\n", wake.roomNum,ctime(&wake.alarmTime));
		     data->expired++;
       		     printf("Expired alarms:\t%d\n", data->expired);
       		     printf("Pending alarms:\t%d\n\n", data->entries - 1);
        	     removeElement(data);
        	 }
	
		 pthread_mutex_unlock(&mutex);
		 
	}
}

//------------------------------------------------------------
//Fuction to cleanup threads and array
//------------------------------------------------------------
void destructor(infoArray * current,pthread_t waker,pthread_t generator)
{
	//free the memory allocated to the array
	free(current->info);

	printf("\nThe Generator thread is cleaning up...\n"); 
	pthread_cancel(generator);
	printf("The Generator thread says goodbye.\n");

	printf("The Waker thread is cleaning up...\n");
	pthread_cancel(waker);
	printf("The Waker thread says goodbye.\n"); 

	pthread_join(generator, NULL);
	pthread_join(waker, NULL);	
	
	//destroy the mutex
	pthread_mutex_destroy(&mutex);    
}
//----------------------------------------------------
int main()
{
	//Create a sigset, empties it and adds SIGINT to it.
	//The program will end when we receive SIGINT
	sigset_t endSig;
	sigemptyset(&endSig);
	sigaddset(&endSig, SIGINT);
	int sig;
	pthread_sigmask(SIG_BLOCK, &endSig, NULL);
	
	//Declares a infoArray pointer
	infoArray *a;
	//Declares a infoArray structure
	infoArray alarmData;
	//initialize the array
        init(&alarmData);
	a = &alarmData;	

	//Declares two threads, one waker and one for generator.
	pthread_t waker;
	pthread_t generator;
	
	//Creates two threads; passing alarmdata as a parameter.
	pthread_create(&waker, NULL, writer, (void *)a);
	pthread_create(&generator, NULL, generate, (void *)a);

	//Waits for the SIGINT signal.
	sigwait(&endSig, &sig);

	destructor(a,waker,generator); 
	return 0;  
}
