#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <chrono>

using namespace std::chrono;
void* sleep_thread(void * arg);
void* run_computation(void * arg);

struct thread_args {
  int id;
  int seconds_to_sleep = 3;
  float sleep_ratio = 0.25;
  int iterations = 10000;
  int * value;
};

int main() 
{

  int num_threads = 3;

  //value to be added up by threads
  int sum_value = 0;

  pthread_t thread_array[num_threads];

  //create all the threads
  for (int i = 0; i < num_threads; i++) {
    struct thread_args *args = new struct thread_args;

    //give an id and the reference to the sum_value to all threads
    args->id = i;
    args->value = &sum_value;

    pthread_create(&thread_array[i], NULL, run_computation, (void *) args);
  }

  //join the threads
  for (int i = 0; i < num_threads; i++) {
    pthread_join(thread_array[i], NULL);
  }

  printf("Finished, computed value:%d",sum_value);
  return 0;
}


void* run_computation(void * arg)
{
    struct thread_args *args = (struct thread_args *)arg;
    printf("Starting computation on Thread:%d\n",args->id);

    auto start = high_resolution_clock::now();
    //loop and add value to sum_value
    for(int i=0;i< args->iterations ;i++){
      *args->value += 1;
    }

    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<nanoseconds>(stop - start);

    long sleep_length = duration.count() * args->sleep_ratio;

    
    struct timespec req= {
       (int)( sleep_length / 1000000000),  
       ( sleep_length % 1000000000) 
    };

    printf("Thread:%d",args->id);
    printf(" Run Length:%lld ",duration.count());
    printf(" Sleep_length:%ld \n",sleep_length);
    nanosleep(&req,NULL);

    printf("Thread returns final computation:%d\n",*args->value);
    return NULL;
}