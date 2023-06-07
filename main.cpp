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
#include <deque>
#include <numeric>
using namespace std::chrono;
typedef uint64_t u64;


//variables to set for testing
int num_threads = 4;
int sleep_length = 1000;
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

//TODO-REMOVE RUN_TIME change type to same as the type in linux(u64 prob)CHANGE TO TUPLE
struct raw_data {
  u64 steal_time;
  u64 preempts;
};

//TODO-remember capacity history instead of steal history and rename. Decay and
//remember latency stddev also add percentage capacity(and actual capacity)
struct profiled_data{
  double stddev;
  double ema;
  std::deque<double> steal_time;
  double preempts_curr;
  double capacity_curr;
  double latency;
};

double calculateStdDev(const std::deque<double>& v) {
    if (v.size() == 0) {
        return 0.0;
    }

    double sum = std::accumulate(v.begin(), v.end(), 0.0);
    double mean = sum / v.size();

    double sq_sum = std::inner_product(v.begin(), v.end(), v.begin(), 0.0);
    double stdDev = std::sqrt(sq_sum / v.size() - mean * mean);

    return stdDev;
}

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

//TODO-Finalize this
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
void get_steal_time_all(int cpunum,std::vector<raw_data>& steal_arr){
  std::ifstream f("/proc/stat");
  std::string s;
  std::getline(f, s);
  for (int i = 0; i < cpunum; i++){
        std::getline(f, s);
        unsigned n;
        std::string l;
        if(std::istringstream(s)>> l >> n >> n >> n >> n >> n >> n >>n >> n )
        {
          steal_arr[i].steal_time = n;
        }
  }
}

//get preemptions of ALL cpus
void get_preempts_all(int cpunum, std::vector<raw_data>& preempt_arr) {
    std::ifstream f("/proc/preempts");
    std::string s;
    int x;
    for (int i = 0; i < cpunum; i++) {
        if (!(f >> s >> x)) {
            std::cerr << "File reading error\n";
            return;
        }
        preempt_arr[i].preempts = x;
    }
}

void get_cpu_information(int cpunum,std::vector<raw_data>& data_arr){
  std::ifstream f("/proc/preempts");
  std::string s;
  u64 preempts;
  u64 steals;
  for (int i = 0; i < cpunum; i++) {
    if(!(f>>preempts>>steals)){
      std::cerr <<"Error";
    }
    data_arr[i].preempts = preempts;
    data_arr[i].steal_time = steals; 
  }

}



//TODO-optimize(With discussed method)	y = pow(0.5, 1/(double)HALFLIFE); https://elixir.bootlin.com/linux/v6.1.31/source/Documentation/scheduler/sched-pelt.c
double calculate_stealtime_ema(const std::deque<double>& steal_history) {


    // Start from the most recent history entry and go back maximally 5 places.
    int max_lookback = std::min(static_cast<int>(steal_history.size()), 5);

    double ema_core = 0.0;
    double weight = 1.0;
    double weight_sum = 0.0;

    for (int lookback = 0; lookback < max_lookback; ++lookback) {
        int index = steal_history.size() - 1 - lookback;
        ema_core += weight * steal_history[index];
        weight_sum += weight;
        weight /= 2.0;
    }
    ema_core /= weight_sum;
    return ema_core;
}

void printResult(int cpunum,profiled_data result[]){
  for (int i = 0; i < cpunum; i++){
        std::cout << "CPU :"<<i<<" Capacity:"<<result[i].capacity_curr<<" Latency:"<<result[i].latency<<" stddev:"<<result[i].stddev;
        std::cout <<" EMA: "<<result[i].ema<<"PREMPTS: "<<result[i].preempts_curr <<std::endl;
  }
}




int main(int argc, char *argv[]) {
  //TODO-Seperate Main method to multiple functions, prefetch-Num_threads
  //default
  int num_threads = 4;
  //in milliseconds
  int sleep_length = 1000;
  int profile_time = 1000;
  //history(amount of runs saved)
  int context_window = 5;
  //options 
  const std::vector<std::string_view> args(argv, argv + argc);
  const bool verbose = has_option(args, "-v");
  const std::string_view str_sleep_time = get_option(args, "-d");
  //TODO-show error codes for incorrectly formatted options
  //TODO-Wrap in function(all options, add help/usage method)
  if(!(str_sleep_time=="")){
    sleep_length = std::stoi(std::string(str_sleep_time));
  }

  const std::string_view str_prfl_time = get_option(args, "-p");
  if(!(str_prfl_time=="")){
    profile_time = std::stoi(std::string(str_prfl_time));
  }
  //get local CPUSET
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  //intialize mutex, threads, and stealtime + runtime trackers
  pthread_t thread_array[num_threads];
  pthread_mutex_t mutex_array[num_threads];
  struct thread_args* args_array[num_threads];

  std::vector<raw_data> data_begin;
  std::vector<raw_data> data_end;
  data_begin.resize(num_threads);
  data_end.resize(num_threads);
  //TODO-homogenize
  profiled_data result_data[num_threads];

  std::deque<std::vector<int>> steal_history;
  pthread_t thId = pthread_self();
  pthread_attr_t thAttr;

  //Fetch highest and lowest possible prios(and set current thread to highest)
  int policy = 0;
  pthread_attr_init(&thAttr);
  //TODO-check that policy is being fetched correctly, and if not fetch.(or set)
  pthread_attr_getschedpolicy(&thAttr, &policy);
  min_prio_for_policy = sched_get_priority_min(policy);
  pthread_setschedprio(thId, sched_get_priority_max(policy));
  pthread_attr_destroy(&thAttr);


  //create all the threads and initilize mutex
  for (int i = 0; i < num_threads; i++) {
    struct thread_args *args = new struct thread_args;
    
    struct sched_param params;
    params.sched_priority = 0;
    //init mutex
    //TODO:use pthread_mutex_init
    mutex_array[i] =  PTHREAD_MUTEX_INITIALIZER;
    //decide which cores to bind cpus too
    //TODO-check this, potentially need to reset i
    CPU_SET(i , &cpuset);
    //give an id and assign mutex to all threads
    args->id = i;
    args->mutex = mutex_array[i];
    //set prio of thread to MIN
    //TODO-error handling for thread creation mistakes
    pthread_create(&thread_array[i], NULL, run_computation, (void *) args);
    pthread_setaffinity_np(thread_array[i], sizeof(cpu_set_t), &cpuset);
    int sch = pthread_setschedparam(thread_array[i], SCHED_IDLE,&params);
  }


  //start profiling+resting loop
  //TODO-Close or start on command;
  while(true) {

    //sleep for sleep_length
    std::this_thread::sleep_for(std::chrono::milliseconds(sleep_length));

    //Set time where threads stop
    endtime = high_resolution_clock::now() + std::chrono::milliseconds(profile_time);

    get_cpu_information(num_threads,data_begin);

    //wake up threads and broadcast 
    initialized = 1;
    pthread_cond_broadcast(&cv);
    //Wait for processors to finish profiling
    //TODO-sleep every x ms and wake up to see if it's now(potentially) (do some testing)
    //set prioclass to SchedRR or schedRT
    std::this_thread::sleep_for(std::chrono::milliseconds(profile_time));
    get_cpu_information(num_threads,data_end);

    for (int i = 0; i < num_threads; i++) {
      int stolen_pass = data_end[i].steal_time - data_begin[i].steal_time;
      int preempts = data_end[i].preempts - data_begin[i].preempts;
      result_data[i].steal_time.push_back(stolen_pass);
      result_data[i].preempts_curr = preempts;
      //TODO-fix this lmao
      result_data[i].capacity_curr = (profile_time-(stolen_pass*10))/(profile_time);
      std::cout<<(profile_time-stolen_pass)<<std::endl;
      std::cout<<profile_time<<std::endl;
      //TODO-use fine grained steal time(custom)
      if(preempts == 0){
        if(stolen_pass != 0){
		  std::cout<< "incompatible steal/preempt"<<std::endl;
	  	return 1;
	    }
        result_data[i].latency = 0;
      } else {
	//TODO-convert to MS per preempt
        result_data[i].latency = stolen_pass/preempts; 
      }
      result_data[i].stddev = calculateStdDev(result_data[i].steal_time);
	//TODO-make sure is capacity/altered function
      result_data[i].ema = calculate_stealtime_ema(result_data[i].steal_time);
    };
    if(verbose){
    printResult(num_threads,result_data);
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
    //TODO-Learn how to use kernel shark to visualize whole process
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
