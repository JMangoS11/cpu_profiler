#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <chrono>
#include <thread>

using namespace std::chrono;
void* sleep_thread(void * arg);
void* run_computation(void * arg);

struct thread_args {
  int id;
  int seconds_to_sleep = 3;
  float sleep_ratio = 0.25;
  int millisecond_duration = 10000;
  int * value;
};

int main() 
{

  int num_threads = 4;
  cpu_set_t cpuset;
  //value to be added up by threads
  int sum_value = 0;

  CPU_ZERO(&cpuset);
  pthread_t thread_array[num_threads];

  //create all the threads
  for (int i = 0; i < num_threads; i++) {
    struct thread_args *args = new struct thread_args;

    //decide which cores to bind cpus too
    CPU_SET(i % 2 * 2, &cpuset);

    //give an id and the reference to the sum_value to all threads
    args->id = i;
    args->value = &sum_value;

    pthread_create(&thread_array[i], NULL, run_computation, (void *) args);
    pthread_setaffinity_np(thread_array[i], sizeof(cpu_set_t), &cpuset);

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

    int addition_calculator = 0;

    //calculating the end time with duration + current time
    auto end = high_resolution_clock::now() + std::chrono::milliseconds(args -> millisecond_duration);


    
    //checking that right now is before the end time
    while(std::chrono::high_resolution_clock::now() < end){
      *args->value += 1;
      addition_calculator += 1;S
    }

    long sleep_length = args -> millisecond_duration * args ->sleep_ratio;
    std::cout << "Thread " << args->id << " Current cpu: " << sched_getcpu() ;
    printf(" Core Speed:%d \n", addition_calculator * 2 / args ->millisecond_duration );

    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_length));

    printf("Thread returns final computation:%d\n",*args->value);
    return NULL;
}