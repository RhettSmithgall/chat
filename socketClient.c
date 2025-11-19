#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

//this struct holds all the arguments required for sendRecieve
struct myStructure {
    int input;
    int port;
    char theirAddr[30];
    int* numberOfThreads;
    clock_t end;
};

//function declaration
void ThreadManager(int,struct myStructure);  //creates threads that run sendReceive
int mainMenu();                              //returns the user input and displays mainmenu
int howManyThreads();                        //manages user input and returns how many threads are needed
void* sendReceive(void*);                    //sends input to the server then prints the output from the server
void helloUser();                            //says hello !
void goodbyeUser();                          //says goodbye :(

int main( int argc, char *argv[]){
    struct myStructure theirAddr;

    //collecting the command line arguments into a struct
    strcpy(theirAddr.theirAddr,argv[1]);
    theirAddr.port = atoi(argv[2]);
    theirAddr.numberOfThreads = 0;

    helloUser();

    //collect user input
    int userInput = mainMenu();

    //this loop collects the users input and how many threads they want
    //then calls threadmanager
    //the loop exits when the user selects "exit" on the main menu
    do{
        theirAddr.input = userInput;
        int howMany = howManyThreads();
        ThreadManager(howMany,theirAddr);
        userInput = mainMenu();
    }while(userInput > 0);

    //goodbye user
    goodbyeUser();
}

//displays the main menu of options then collects the users selection
//aceepts no arguments and returns the user input
int mainMenu(){
    int userInput;

    printf("|---------------------|\n");
    printf("|     Main Menu       |\n");
    printf("|---------------------|\n");
    printf("|0. Exit Program      |\n");
    printf("|1. Date and time     |\n");
    printf("|2. Uptime            |\n");
    printf("|3. Memory User       |\n");
    printf("|4. NetStat           |\n");
    printf("|5. Current Users     |\n");
    printf("|5. Running Processes |\n");
    printf("|7. Close Server      |\n");
    printf("|---------------------|\n");
    printf("Enter your selection:");

    scanf("%i",&userInput);
    return userInput;
}

//collects the user input for how many threads they want to run
//accepts no arguments and returns the users input
int howManyThreads(){
    int userInput;

    printf("\nHow many times would you like to run this query?\n");
    printf("Enter your selection:");
    scanf("%i",&userInput);
    return userInput;
}

//sendreceive is the thread function the threadmanager runs
//because pthreads only accepts a void pointer to a function it's type and argument are both void pointers
//this function creates a socket, sends the user input, recieves the output from the server, then prints the result(s)
//until the socket is closed by the server
void *sendReceive(void *input)

{
    int sockfd;

    //derefrencing our void pointer structure with all the port and ip address information
    struct myStructure *theirAddr = (struct myStructure*)input;
    struct sockaddr_in dest_addr;

    //create socket
    if((sockfd = socket(PF_INET,SOCK_STREAM, 0)) < 0){
        perror("Error creating socket");
        exit(1);
    }

    //populate socket with addresses
    dest_addr.sin_family = AF_INET;                     // host byte order
    dest_addr.sin_port = htons(theirAddr->port);        // short, network byte order
    dest_addr.sin_addr.s_addr = inet_addr(theirAddr->theirAddr); //ip address
    memset(&(dest_addr.sin_zero), '\0', 8);             // zero the rest of the struct

    //connect and wait for accept() on the server side
    if(connect(sockfd, (struct sockaddr *)&dest_addr, sizeof(struct sockaddr)) < 0){
        perror("Connection error");
    }

    int numbytes = 0;
    char buffer[128];
    int userInput[10];

    //get user input from the struct
    userInput[0] = theirAddr->input;

    //send input to server
    if (send(sockfd,userInput,sizeof(userInput), 0) == -1){
        perror("send");
        exit(1);
    }

    while(numbytes = recv(sockfd,buffer,sizeof(buffer),0) > 0){ //recieve and print output from server
        printf("recieving: %s",buffer); 
    }
    if (numbytes == -1) {
        perror("recv"); 
    }
    else if (numbytes == 0) {//close socket when connection has been terminated
        close(sockfd); 
    }

    //c gets a little difficult to read here, the following code is derefrencing a struct pointer back into
    //the value of the integer in order to deincriment by one
    //pthread functions can't return a value so this direct manipulation of the address we need is required
    *(theirAddr->numberOfThreads) = (*(theirAddr->numberOfThreads) - 1);

    //here at the end the thread is making a note back in thread manager of the time it finished
    theirAddr->end = clock();

    //and finally it cancels itself to free up a new thread to be used
    pthread_cancel(pthread_self());
}

void ThreadManager(int howMany,struct myStructure inputAddr)
{
    pthread_t thread_id[howMany];
    float averageTime;

    //each thread needs to be able to record it's own individual time to complete so
    //an array of structs are created to be arguments for our threaded function
    struct myStructure theirAddr[howMany];
    int numberOfThreads = 0;

    //because clock(); can be innacurate the timespec framework is used
    float begin[howMany + 1], end[howMany + 1];

    //collect starting time
    begin[0] = clock();

    //create threads
    for(int i = 0;i < howMany;i++)
    {
        //the machines these run on have 12 cores, so no more than 12 threads are reccomended
        //this code ensures that the client will wait for more threads to be avaible before continuing
        if(numberOfThreads < 13) {
            theirAddr[i] = inputAddr;
            //the address of numberOfThreads here is sent along as an argument for our threaded function so it can directly
            //call back and release a new thread to be used
            theirAddr[i].numberOfThreads = &numberOfThreads;
            begin[i+1] = clock();
            pthread_create(&thread_id[i],NULL,sendReceive,(void *)&theirAddr[i]);
            numberOfThreads++;
        }
        else
        {
            //if a thread is not avaible, the loop continues, running in place until a thread frees up
            //there is a loss in performace here, and further improvements could be made to actually make the computer wait instead
            //of looping, however common functions like wait(); aren't good enough for the job, something far more complicated would be required
            i--;
        }
    }

    //ensure threads complete
    for(int i = 0;i < howMany;i++)
    {
        pthread_join(thread_id[i],NULL);
        end[i+1] = theirAddr[i].end;
        averageTime += (end[i+1] - begin[i+1])/(CLOCKS_PER_SEC/1000);
    }

    //display all the threads time to complete
    printf("|-------------------------|\n");
    printf("| Thread | runtime        |\n");
    printf("|   no.  | (miliseconds)  |\n");
    printf("|-------------------------|\n");
    for(int i = 0;i < howMany;i++)
    {
    printf("| #%5i | %-12.2fms |\n",i+1,(end[i+1] - begin[i+1])/(CLOCKS_PER_SEC/1000));
    }

    //collect ending time
    end[0] = clock();

    //by dividing by the number of threads we get the average
    printf("|---------------------------------------------|\n");
    printf("| Average     | %-8.2fms                    |\n",(averageTime/howMany));
    printf("|---------------------------------------------|\n");
    //timespec gives time in seconds so the result is multipled by 1000 to create milliseconds
    printf("| Total time  | %-8.2fms                    |\n",(end[0] - begin[0])/(CLOCKS_PER_SEC/1000));
    printf("|---------------------------------------------|\n");
}

//say hello
void helloUser(){
    printf("|----------------------------------|\n");
    printf("| Rhett Smithgall & George Shannon |\n");
    printf("|      multithreaded client        |\n");
    printf("|----------------------------------|\n");
}

//say goodbye
void goodbyeUser(){
    printf("goodbye user!\n");
    //Autograph
    printf("Programmer:\n");
    printf(""
            "    ____  __         __  __     _____           _ __  __                ____    \n"
            "   / __ \\/ /_  ___  / /_/ /_   / ___/____ ___  (_) /_/ /_  ____ _____ _/ / /    \n"
            "  / /_/ / __ \\/ _ \\/ __/ __/   \\__ \\/ __ `__ \\/ / __/ __ \\/ __ `/ __ `/ / /     \n"
            " / _, _/ / / /  __/ /_/ /_    ___/ / / / / / / / /_/ / / / /_/ / /_/ / / /      \n"
            "/_/ |_/_/ /_/\\___/\\__/\\__/   /____/_/ /_/ /_/_/\\__/_/ /_/\\__, /\\__,_/_/_/       \n"
            "                                                        /____/                  \n");
}