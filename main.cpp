#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
using namespace std::chrono;

void* sleep_thread(void * arg);
void* run_computation(void * arg);

// our global condition variable and mutex
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

int sleep_length =1;
int initialized = 0;

struct thread_args {
  int id;
  int test_duration = 950;
  int start_time = 0;
  int end_time = 0;
  float speed = 0;
  pthread_mutex_t mutex;
};

int main() 
{

  int num_threads = 4;

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  unsigned n;
  if(std::ifstream("/proc/stat").ignore(3) >> n >> n >> n >> n >> n >> n >> n)
    {
        // use n here...
        std::cout << n << '\n';
    }
  pthread_t thread_array[num_threads];
  pthread_mutex_t mutex_array[num_threads];
  struct thread_args* args_array[num_threads];


  //create all the threads and initilize mutex
  for (int i = 0; i < num_threads; i++) {
    struct thread_args *args = new struct thread_args;
    
    //init mutex
    mutex_array[i] =  PTHREAD_MUTEX_INITIALIZER;
    //decide which cores to bind cpus too
    CPU_SET(i , &cpuset);

    //give an id to all threads
    args->id = i;
    args->mutex = mutex_array[i];


    pthread_create(&thread_array[i], NULL, run_computation, (void *) args);
    pthread_setaffinity_np(thread_array[i], sizeof(cpu_set_t), &cpuset);
    args_array[i] = args;
  }



  while(true){
    printf("sleeping \n");
    sleep(sleep_length);
    initialized = 1;

    //print speed from LAST run
    for (int i = 0; i < num_threads; i++) {
      std::cout<< "Thread:"<<args_array[i]->id << " Speed:" <<args_array[i]->speed <<" Computation_start:"<<args_array[i]->start_time % 10000 << " Computation_end:" << args_array[i]->end_time % 10000 << std::endl ;

    };
    printf("awakening \n");
    pthread_cond_broadcast(&cv);
  }

  //join the threads
  for (int i = 0; i < num_threads; i++) {
    pthread_join(thread_array[i], NULL);
  }

  printf("Process Finished");
  return 0;
}

int get_steal_time(int cpunum){
  std::ifstream f("/proc/stat");
  std::string s;
  for (int i = 0; i <= cpunum; i++){
        std::getline(f, s);
  }
  unsigned n;
  std::string l;
  if(std::istringstream(s)>> l >> n >> n >> n >> n >> n >> n >>n >>n )
    {
        // use n here...
        return(n);
    }

    return 0;
}


void* run_computation(void * arg)
{

    struct thread_args *args = (struct thread_args *)arg;
    while(true){
      pthread_mutex_lock(&args->mutex);
      initialized = 0;
      while (! initialized) {
      pthread_cond_wait(&cv, &args->mutex);
      }
      pthread_mutex_unlock(&args->mutex);
      
      int addition_calculator = 0;

      //calculating the end time with duration + current time
      auto end = high_resolution_clock::now() + std::chrono::milliseconds(args -> test_duration);

      int start_steal = get_steal_time(args->id);
      int ms = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
      args->start_time = ms;
      //checking that right now is before the end time
      while(std::chrono::high_resolution_clock::now() < end){
        addition_calculator += 1;
      }
      ms = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count() ;
      args->end_time = ms;
      
      args->speed = get_steal_time(args->id) - start_steal;
      }
      return NULL;
} 