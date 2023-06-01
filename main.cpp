#include <iostream>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>
#include <sys/time.h>
#include <sys/resource.h>
#include <vector>
#include <sys/types.h>
#include <sys/syscall.h>
#include <cstdlib>
#include <sys/vfs.h>
#include <cmath>
using namespace std::chrono;

//variables to set for testing
int num_threads = 4;
int sleep_length = 10;
int profile_time = 1000;
int context_window = 5;
//initialize global variables
int initialized = 0;
std::chrono::time_point<std::chrono::_V2::system_clock, std::chrono::_V2::system_clock::duration> endtime;
int min_prio_for_policy;
void* sleep_thread(void * arg);
void* run_computation(void * arg);
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;


//Arguments for each thread
struct thread_args {
  int id;
  pthread_mutex_t mutex;
};


std::string_view get_option(
    const std::vector<std::string_view>& args, 
    const std::string_view& option_name) {
    for (auto it = args.begin(), end = args.end(); it != end; ++it) {
        if (*it == option_name)
            if (it + 1 != end)
                return *(it + 1);
    }
    
    return "";
};

bool has_option(
    const std::vector<std::string_view>& args, 
    const std::string_view& option_name) {
    for (auto it = args.begin(), end = args.end(); it != end; ++it) {
        if (*it == option_name)
            return true;
    }
    
    return false;
};

//To get steal time of a CPU
int get_steal_time(int cpunum) {
  std::ifstream f("/proc/stat");
  std::string s;
  for (int i = 0; i <= cpunum + 1; i++) {
        std::getline(f, s);
  }
  unsigned n;
  std::string l;
  if(std::istringstream(s)>>l>> n >> n >> n >> n >> n >> n >> n >> n ) {
        return(n);
  }
  return 0;
}

std::string get_cgroup_version() {
  std::ifstream cgroup_controllers("/sys/fs/cgroup/cgroup.controllers");
  if (cgroup_controllers.is_open()) {  
    return "cgroup2fs";
  } else {
    // Check if the cpu controller is available for cgroup v1
    std::ifstream cpu_controller("/sys/fs/cgroup/cpu");
    if (cpu_controller.is_open()) {
      return "tmpfs";
    }
  }
  return "unknown";
}

bool set_pthread_to_low_prio_cgroup(pthread_t thread) {
    // Get the Linux thread ID.
    pid_t tid = syscall(SYS_gettid);

    // Determine the cgroup version.
    std::string cgroup_version = get_cgroup_version();

    // Compose the path to the cgroup tasks or cgroup.procs file.
    std::string tasks_file_path;
    if (cgroup_version == "tmpfs") {
        tasks_file_path = "/sys/fs/cgroup/cpu/Low_prio_cgroup/tasks";
    } else if (cgroup_version == "cgroup2fs") {
        tasks_file_path = "/sys/fs/cgroup/Low_prio_cgroup/cgroup.procs";
    } else {
        std::cerr << "Unsupported cgroup version" <<cgroup_version << std::endl;
        return false;
    }

    // Open the tasks or cgroup.procs file for writing.
    std::ofstream tasks_file(tasks_file_path, std::ios_base::app);
    if (!tasks_file.is_open()) {
        std::cerr << "Failed to open cgroup tasks file: " << tasks_file_path << std::endl;
        return false;
    }

    // Write the thread ID to the tasks or cgroup.procs file.
    tasks_file << tid;
    if (tasks_file.fail()) {
        std::cerr << "Failed to write to cgroup tasks file: " << tasks_file_path << std::endl;
        return false;
    }

    tasks_file.close();
    return true;
}


//To get steal time of ALL CPUs
void get_steal_time_all(int cpunum,int steal_arr[]){
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
        steal_arr[i] = n;
        }
  }
}

//get run time of ALL cpus
void get_run_time_all(int cpunum,int run_arr[]){
  std::ifstream f("/proc/stat");
  std::string s;
  int output[cpunum];
  
  std::getline(f, s);
  for (int i = 0; i < cpunum; i++){
        std::getline(f, s);
        unsigned n;
        std::string l;
        if(std::istringstream(s)>> l >> n)
        {
        // use n here...
        run_arr[i] = n;
        }
  }
}

std::vector<double> calculate_stealtime_ema(const std::vector<std::vector<int>>& steal_history) {
    const int num_cores = steal_history.at(0).size();

    std::vector<double> ema(num_cores, 0.0);

    // Start from the most recent history entry and go back maximally 5 places.
    int max_lookback = std::min(static_cast<int>(steal_history.size()), 5);

    for (int core = 0; core < num_cores; ++core) {
        double ema_core = 0.0;
        double weight = 1.0;
        double weight_sum = 0.0;

        for (int lookback = 0; lookback < max_lookback; ++lookback) {
            int index = steal_history.size() - 1 - lookback;
            ema_core += weight * steal_history[index][core];
            weight_sum += weight;
            weight /= 2.0;
        }

        ema_core /= weight_sum;
        ema[core] = ema_core;
    }

    return ema;
}


void print_steal_times_and_ema(const std::vector<double>& ema_array, const std::vector<std::vector<int>>& steal_history) {
    for (int core = 0; core < steal_history[0].size(); ++core) {
        std::vector<int> core_steal_history;
        for (int i = 0; i < steal_history.size(); ++i) {
            core_steal_history.push_back(steal_history[i][core]);
        }

        double ema = ema_array[core];

        std::cout << "StealTime Core " << core + 1 << ": ";
        for (int i = std::max(0, static_cast<int>(core_steal_history.size()) - 5); i < core_steal_history.size(); ++i) {
            std::cout << core_steal_history[i];
            if (i < core_steal_history.size() - 1) {
                std::cout << ", ";
            }
        }
        std::cout << " EMA: " << ema << std::endl;
    }
}

int main(int argc, char *argv[]) {
  //options 
  const std::vector<std::string_view> args(argv, argv + argc);
  const bool verbose = has_option(args, "-v");
  const std::string_view sleep_time = get_option(args, "-d");
  std::cout << std::string(sleep_time) << std::endl;
  std::cout<< (std::stoi(std::string(sleep_time))+3) << std::endl;
  //get local CPUSET
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  //intialize mutex, threads, and stealtime + runtime trackers
  pthread_t thread_array[num_threads];
  pthread_mutex_t mutex_array[num_threads];
  struct thread_args* args_array[num_threads];
  int steal_time_end[num_threads];
  int steal_time_begin[num_threads];
  std::vector<std::vector<int>> steal_history;
  int run_time_end[num_threads];
  int run_time_begin[num_threads];

  pthread_t thId = pthread_self();
  pthread_attr_t thAttr;
  
  //Fetch highest and lowest possible prios(and set current thread to highest)
  int policy = 0;
  int max_prio_for_policy = 0;
  pthread_attr_init(&thAttr);
  pthread_attr_getschedpolicy(&thAttr, &policy);
  min_prio_for_policy = sched_get_priority_min(policy);
  int d = pthread_setschedprio(thId, sched_get_priority_max(policy));
  if(d == 0){
      std::cout<<min_prio_for_policy<<std::endl;
    }else{
      printf("FAILED\n");
    };
  pthread_attr_destroy(&thAttr);


  //create all the threads and initilize mutex
  for (int i = 0; i < num_threads; i++) {
    struct thread_args *args = new struct thread_args;
    struct sched_param params;

    params.sched_priority = 0;
    //init mutex
    mutex_array[i] =  PTHREAD_MUTEX_INITIALIZER;
    //decide which cores to bind cpus too
    CPU_SET(i , &cpuset);
    //give an id and assign mutex to all threads
    args->id = i;
    args->mutex = mutex_array[i];
    //set prio of thread to MIN
    
    pthread_create(&thread_array[i], NULL, run_computation, (void *) args);
    int sch = pthread_setschedparam(thread_array[i], SCHED_IDLE,&params);
    args_array[i] = args;
  
  }


  //start profiling+resting loop
  while(true) {

    //sleep for sleep_length
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_length));

    //Set time where threads stop
    endtime = high_resolution_clock::now() + std::chrono::milliseconds(profile_time);
    
    //wake up threads and broadcast 
    initialized = 1;
    pthread_cond_broadcast(&cv);
    get_steal_time_all(num_threads,steal_time_begin);
    get_run_time_all(num_threads,run_time_begin);
    
    //Wait for processors to finish profiling
    std::this_thread::sleep_for(std::chrono::milliseconds(profile_time));

    get_steal_time_all(num_threads,steal_time_end);
    get_run_time_all(num_threads,run_time_end);
    
    std::vector<int> current_steals;
    for (int i = 0; i < num_threads; i++) {
      int stolentime = steal_time_end[i]-steal_time_begin[i];
      int rantime = run_time_end[i]-run_time_begin[i];
      current_steals.push_back(stolentime);
    };
    
    steal_history.push_back(current_steals);
    std::vector<double> ema = calculate_stealtime_ema(steal_history);
    if(verbose) {
      print_steal_times_and_ema(ema, steal_history);
    }
  }

  //join the threads
  for (int i = 0; i < num_threads; i++) {
    pthread_join(thread_array[i], NULL);
  }
  printf("Process Finished");
  return 0;
}



int get_profile_time(int cpunum) {
  std::ifstream f("/proc/stat");
  std::string s;
  for (int i = 0; i <= cpunum; i++) {
    std::getline(f, s);
  }
  unsigned n;
  std::string l;
  if(std::istringstream(s)>> l >> n >> n >> n ) {
    return(n);
  }
  return 0;
}

void* run_computation(void * arg)
{
    struct thread_args *args = (struct thread_args *)arg;
    while(true) {
      pthread_mutex_lock(&args->mutex);
      
      while (! initialized) {
      pthread_cond_wait(&cv, &args->mutex);
      }
      pthread_mutex_unlock(&args->mutex);

      int addition_calculator = 0;
      while(std::chrono::high_resolution_clock::now() < endtime) {
        addition_calculator += 1;
      };
      initialized = 0;
      }
      return NULL;
} 
