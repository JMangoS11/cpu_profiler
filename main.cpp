#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <chrono>
#include <thread>

using namespace std::chrono;
void* sleep_thread(void * arg);
void* run_computation(void * arg);
// our global condition variable and mutex
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

int sleep_length = 3;

int initialized = 0;
struct thread_args {
  int id;
  int test_duration = 500;
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

  pthread_mutex_unlock(&mtx);
  //create all the threads
  for (int i = 0; i < num_threads; i++) {
    struct thread_args *args = new struct thread_args;

    //decide which cores to bind cpus too
    CPU_SET(i , &cpuset);

    //give an id and the reference to the sum_value to all threads
    args->id = i;
    args->value = &sum_value;

    pthread_create(&thread_array[i], NULL, run_computation, (void *) args);
    pthread_setaffinity_np(thread_array[i], sizeof(cpu_set_t), &cpuset);

  }

  while(true){
    printf("sleeping \n");
    sleep(sleep_length);
    initialized = 1;
    printf("awakening \n");
    pthread_cond_broadcast(&cv);
    pthread_mutex_unlock(&mtx);
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
    while(true){
      pthread_mutex_lock(&mtx);
      initialized = 0;
      while (! initialized) {
      pthread_cond_wait(&cv, &mtx);
      }
      pthread_mutex_unlock(&mtx);
      
      int addition_calculator = 0;

      //calculating the end time with duration + current time
      auto end = high_resolution_clock::now() + std::chrono::milliseconds(args -> test_duration);


      int ms = duration_cast< milliseconds >(
    system_clock::now().time_since_epoch()
).count();
      std::cout << "Thread " << args->id << " starts at :" << ms % 1000;
      //checking that right now is before the end time
      while(std::chrono::high_resolution_clock::now() < end){
        *args->value += 1;
        addition_calculator += 1;
      }
      ms = duration_cast< milliseconds >(
    system_clock::now().time_since_epoch()
).count() ;
      std::cout << "Thread " << args->id << " ends at :" << ms % 1000;
      std::cout << "Thread " << args->id << " Current cpu: " << sched_getcpu() ;
      printf(" Core Speed:%d \n", addition_calculator / args -> test_duration );
    }
    return NULL;
}