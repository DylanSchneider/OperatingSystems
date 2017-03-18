//
//  main.c
//  multi-lookup
//
//  Created by Dylan Schneider on 3/12/17.
//  Copyright © 2017 Dylan Schneider. All rights reserved.
//
#include "multi-lookup.h"

int FILES_FINISHED;
int NUM_INPUT_FILES;
char* OUT_FILE;
int THREAD_MAX;

queue q;
pthread_cond_t full;
pthread_cond_t empty;

pthread_mutex_t queue_lock;
pthread_mutex_t FF_lock;
pthread_mutex_t out_lock;


void* readerPool(char** inFiles){
    //create number of reader threads same number as number of input files
    pthread_t reader_threads[NUM_INPUT_FILES];
    for (int i=0; i < NUM_INPUT_FILES; i++){
        pthread_create(&reader_threads[i], NULL, (void*) Read, (void*)inFiles[i]);
        pthread_join(reader_threads[i], NULL);
    }
    return NULL;
}

void* Read(char* fileName){
    //read file line by line
    //put each line (domain) in the queue.
    FILE* input = fopen(fileName, "r");
    
    if(!input){
        perror("Error opening input file.\n");
        return NULL;
    }
    char domain[DOMAIN_SIZE];
    while(fscanf(input, INPUTFS, domain) > 0){
        
        pthread_mutex_lock(&queue_lock);
        
        while(queue_is_full(&q)){
            pthread_cond_wait(&full, &queue_lock);
        }
        
        // Increment counter to count how many hostnames we are pushing to the queue
        queue_push(&q, strdup(domain));
        
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&queue_lock);
    }
    
    pthread_mutex_lock(&FF_lock);
    FILES_FINISHED++;
    pthread_mutex_unlock(&FF_lock);
    
    fclose(input);
    return NULL;
}




void* resolverPool(){
    //create number of resolver threads same number as number of cores on system
    pthread_t consumer_threads[THREAD_MAX];
    for (int i=0; i < THREAD_MAX; i++){
        pthread_create(&consumer_threads[i], NULL, (void*) Resolve, NULL);
        pthread_join(consumer_threads[i], NULL);
    }
    return NULL;
}

void* Resolve(){
    //open shared out file
    FILE* output_file = fopen(OUT_FILE, "a");
    if(!output_file){
        perror("Error opening output file");
        return NULL;
    }
    //Loops infinitely to support unlimited files
    //Breaks when files finished is equal to total number of text files
    while(1){
        
        pthread_mutex_lock(&queue_lock);

        // Check if all files done
        while(queue_is_empty(&q)){
            //check if we're done
            pthread_mutex_lock(&FF_lock); //critical section
            int done = 0;
            if(FILES_FINISHED == NUM_INPUT_FILES){
                done = 1;
            }
            pthread_mutex_unlock(&FF_lock);
            if (done){
                //while loop and function finish here only
                pthread_mutex_unlock(&queue_lock);
                fclose(output_file);
                return NULL;
            }
            //if not done, wait until something is enqueued
            pthread_cond_wait(&empty, &queue_lock); //wait on empty, but release the queue while we wait
        }
        //queue pop
        char* single_hostname = (char*) queue_pop(&q);
        
        pthread_cond_signal(&full); //after popping, we will never be full
        
        pthread_mutex_unlock(&queue_lock);
        
        char* IPs[30]; //all IPs of each domain stored here
        for(int i = 0; i < 30; i++){
            IPs[i] = NULL;
        }
        
        //DNS resolution
        if(dnslookup(single_hostname, IPs) == UTIL_FAILURE){
            //on a bogus domain, copy "none" to IP list
            fprintf(stderr, "dns lookup error hostname: %s\n", single_hostname);
            for(int i = 0; i < 30; i++){
                if(IPs[i] == NULL){
                    char none[4] = "none";
                    IPs[i] = (char*) malloc(sizeof(none));
                    strncpy(IPs[i], none, sizeof(none));
                    break;
                }
            }
        };
        
        pthread_mutex_lock(&out_lock); //critical section
        
        fprintf(output_file, "%s,", single_hostname); //write the domain name to file
        for(int i = 0; i < 30; i++){ //write each IP to the same line as hostname
            if(IPs[i+1] == NULL){
                fprintf(output_file, "%s\n", IPs[i]);
                break;
            }
            else{
                fprintf(output_file, "%s,", IPs[i]);
            }
        }
        
        pthread_mutex_unlock(&out_lock);
        
        //free mallocs
        free(single_hostname);
        for(int i = 0; i < 30; i++){
            free(IPs[i]);
        }
    }
}




int main(int argc, char * argv[]) {
    //create pthread pools
    FILES_FINISHED = 0;
    NUM_INPUT_FILES = argc-2;
    OUT_FILE = argv[argc-1];
    THREAD_MAX = sysconf(_SC_NPROCESSORS_ONLN);
    
    //initialize queue and locks
    queue_init(&q, 0); //invoke MAX_QUEUE_SIZE for the queue
    
    pthread_cond_init(&empty, NULL);
    pthread_cond_init(&full, NULL);
    pthread_mutex_init(&queue_lock, NULL);
    pthread_mutex_init(&FF_lock, NULL);
    pthread_mutex_init(&out_lock, NULL);
    
    //incorrect usage
    if(argc < MINARGS){
        fprintf(stderr, "Not enough arguments: %d, must have at least 2 (one input file, and one output file).\n", (argc - 1));
        fprintf(stderr, "Usage: %s <inputFilePath> <inputFilePath> ... <outputFilePath>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    //list of input files
    char* in_files[NUM_INPUT_FILES];
    for (int i=0; i < NUM_INPUT_FILES; i++){
        in_files[i] = argv[i+1];
    }
    
    //thread pools
    pthread_t producer, consumer;
    int createProducer = pthread_create(&producer, NULL, (void*) readerPool, in_files);
    int createConsumer = pthread_create(&consumer, NULL, (void*) resolverPool, NULL);
    
    if(createProducer || createConsumer)
        fprintf(stderr, "Error creating initial threads.");
    
    pthread_join(producer, NULL);
    pthread_join(consumer, NULL);
    
    //free queue and locks
    queue_cleanup(&q);
    
    pthread_cond_destroy(&empty);
    pthread_cond_destroy(&full);
    pthread_mutex_destroy(&queue_lock);
    pthread_mutex_destroy(&FF_lock);
    pthread_mutex_destroy(&out_lock);
    
    return EXIT_SUCCESS;
}
