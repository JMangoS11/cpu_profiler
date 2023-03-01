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

int sleep_length = 1;
int initialized = 0;
int computation_time = 100;
std::chrono::time_point<std::chrono::_V2::system_clock, std::chrono::_V2::system_clock::duration> endtime;
int min_prio_for_policy;

struct thread_args {
  int id;
  int rest_duration = 950;
  int start_time = 0;
  int end_time = 0;
  float steal_time = 0;
  pthread_mutex_t mutex;
};

struct thread_data {
  int id;
  int steal_time;
};

int get_steal_time(int cpunum){
  std::ifstream f("/proc/stat");
  std::string s;
  for (int i = 0; i <= cpunum + 1; i++){
        std::getline(f, s);
  }
  unsigned n;
  std::string l;
  if(std::istringstream(s)>> l >> n >> n >> n >> n >> n >> n >>n >> n )
    {
        // use n here...
        
        return(n);
    }

    return 0;
}


void get_steal_time_all(int cpunum,int myArray[]){
  std::ifstream f("/proc/stat");
  std::string s;
  int output[cpunum];
  
  std::getline(f, s);
  for (int i = 0; i < cpunum; i++){
        std::getline(f, s);
        unsigned n;
        std::string l;
        if(std::istringstream(s)>> l >> n >> n >> n >> n >> n >> n >>n >> n )
        {
        // use n here...
        myArray[i] = n;
        }
  }
}

int main() 
{

  int num_threads = 4;
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  pthread_t thread_array[num_threads];
  pthread_mutex_t mutex_array[num_threads];
  struct thread_args* args_array[num_threads];
  int steal_time_end[num_threads];
  int steal_time_begin[num_threads];

  //set controller thread to be highest priority

  pthread_t thId = pthread_self();
  pthread_attr_t thAttr;
  int policy = 0;
  int max_prio_for_policy = 0;

  pthread_attr_init(&thAttr);
  pthread_attr_getschedpolicy(&thAttr, &policy);
  max_prio_for_policy = sched_get_priority_max(policy);
  min_prio_for_policy = sched_get_priority_min(policy);
  pthread_setschedprio(thId, max_prio_for_policy);
  pthread_attr_destroy(&thAttr);



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
    //SLEEP SIGNALS
    sleep(sleep_length);
    

    printf("profiling \n");
    
    int starttime = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count() % 10000; 
    endtime = high_resolution_clock::now() + std::chrono::milliseconds(computation_time);
    //WAKE UP SIGNALS
    
    
    initialized = 1;
    pthread_cond_broadcast(&cv);
    get_steal_time_all(num_threads,steal_time_begin);
    //GO BACK TO SLEEP FOR THIS amount of time
    while(std::chrono::high_resolution_clock::now() < endtime){
    }
    get_steal_time_all(num_threads, steal_time_end);
    auto end = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count() % 10000; 
    for (int i = 0; i < num_threads; i++) {
      int stolentime = steal_time_end[i]-steal_time_begin[i];
      std::cout<< "Thread:"<<args_array[i]->id << " Steal Time:" << stolentime <<" Start Time:"<< args_array[i]->start_time  % 10000<< " Start Time Main:" << starttime << " End Time:" << args_array[i]->end_time  % 10000<< " End Time Main:" << end << std::endl ;
    };
  }

  //join the threads
  for (int i = 0; i < num_threads; i++) {
    pthread_join(thread_array[i], NULL);
  }

  printf("Process Finished");
  return 0;
}



int get_computation_time(int cpunum){
  std::ifstream f("/proc/stat");
  std::string s;
  for (int i = 0; i <= cpunum; i++){
        std::getline(f, s);
  }
  unsigned n;
  std::string l;
  if(std::istringstream(s)>> l >> n )
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
      
      while (! initialized) {
      pthread_cond_wait(&cv, &args->mutex);
      }
      pthread_mutex_unlock(&args->mutex);
      
      int addition_calculator = 0;


      //int start_steal = get_steal_time(args->id);
      //int start_computation = get_computation_time(args->id);

      //int ms = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
      args->start_time = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count() % 10000;

      while(std::chrono::high_resolution_clock::now() < endtime &&  initialized){
        
        addition_calculator += 1;
      };
      args->end_time = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count() % 10000;
     // ms = duration_cast< milliseconds >(system_clock::now().time_since_epoch()).count();
      initialized = 0;
      }
      return NULL;
} 