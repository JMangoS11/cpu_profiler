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


//initialize global variables
int num_threads = 4;
int sleep_length = 1000;
int profile_time = 100;

//this is for saving how many profiling periods have gone by, so far.
int profiler_iter = 0;

//this decides how many regular profile intervals go by before a "heavy" profile happens, where we try to get the actual capacity of the core
int heavy_profile_interval = 5;
int context_window = 5;
double milliseconds_totick_factor = static_cast<double>(sysconf(_SC_CLK_TCK))/1000.0;
bool verbose = false;
int initialized = 0;
std::chrono::time_point<std::chrono::_V2::system_clock, std::chrono::_V2::system_clock::duration> endtime;
void* run_computation(void * arg);
pthread_cond_t cv = PTHREAD_COND_INITIALIZER;


//Arguments for each thread
struct thread_args {
  int id;
  int tid = -1;
  pthread_mutex_t mutex;
  u64 *addition_calc;
  double user_time;
};



struct raw_data {
  u64 steal_time;
  u64 preempts;
  u64 raw_compute;
  u64 use_time;
};



struct profiled_data{
  double capacity_perc_stddev;
  double capacity_adj_stddev;
  double latency_stddev;
  double preempts_stddev;

  double capacity_perc_ema;
  double latency_ema;
  double preempts_ema;

  double capacity_perc_ema_a;
  double capacity_adj_ema_a;
  double latency_ema_a;
  double preempts_ema_a;

  std::deque<double> capacity_perc_hist;
  std::deque<double> capacity_adj_hist;
  std::deque<double> latency_hist;
  std::deque<double> preempts_hist;

  double preempts;
  double capacity_perc;
  double capacity_adj;
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

u64 getThreadCpuTime(pid_t tid) {
    std::ifstream stat_file("/proc/self/task/" + std::to_string(tid) + "/stat");
    if (!stat_file) {
        std::cerr << "Could not open stat file for thread " << tid << std::endl;
        return -1;
    }

    // Skip the first 13 fields
    std::string tmp;
    for (int i = 0; i < 13; ++i) {
        stat_file >> tmp;
    }

    u64 utime, stime;
    if (!(stat_file >> utime >> stime)) {
        std::cerr << "Could not read utime and stime for thread " << tid << std::endl;
        return -1;
    }

    return utime + stime;
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



void moveThreadtoLowPrio(pid_t tid) {

    std::string path = "/sys/fs/cgroup/lw_prgroup/cgroup.threads";
    std::ofstream ofs(path, std::ios_base::app);
    if (!ofs) {
        std::cerr << "Could not open the file\n";
        return;
    }
    ofs << tid << "\n";
    ofs.close();
    struct sched_param params;
    params.sched_priority = sched_get_priority_min(SCHED_IDLE);
    sched_setscheduler(tid,SCHED_IDLE,&params);
}

void moveThreadtoHighPrio(pid_t tid) {

    std::string path = "/sys/fs/cgroup/hi_prgroup/cgroup.threads";
    std::ofstream ofs(path, std::ios_base::app);
    if (!ofs) {
        std::cerr << "Could not open the file\n";
        return;
    }
    ofs << tid << "\n";
    ofs.close();
}

void moveCurrentThread() {
    pid_t tid;
    tid = syscall(SYS_gettid);
    
    std::string path = "/sys/fs/cgroup/cgroup.procs";
    std::ofstream ofs(path, std::ios_base::app);
    if (!ofs) {
        std::cerr << "Could not open the file\n";
        return;
    }
    ofs << tid << "\n";
    ofs.close();
    struct sched_param params;
    params.sched_priority = sched_get_priority_max(SCHED_RR);
    sched_setscheduler(tid,SCHED_RR,&params);
}

u64 timespec_to_ns(struct timespec* ts) {
    return ((uint64_t) ts->tv_sec * 1000000000ULL) + (uint64_t) ts->tv_nsec;
}

void get_cpu_information(int cpunum,std::vector<raw_data>& data_arr,std::vector<thread_args*> thread_arg){
  std::ifstream f("/proc/preempts");
  std::string s;
  u64 preempts;
  u64 steals;
  for (int i = 0; i < cpunum; i++) {
    std::getline(f,s);
    std::getline(f,s);
    data_arr[i].preempts = std::stoull(s);
    std::getline(f,s);
    data_arr[i].steal_time = std::stoull(s);

  }
}



double calculate_ema(double decay_factor, double& ema_help, double prev_ema,double new_value) {
  double newA = (1+decay_factor*ema_help);
  double result = (new_value + ((prev_ema)*ema_help*decay_factor))/newA;
  ema_help = newA;
  return result;
}


//helper function to set context window to be short
void addToHistory(std::deque<double>& history_list,double item){
  if(history_list.size() > context_window) {
    history_list.pop_front();
  }
  history_list.push_back(item);
}

void setArguments(const std::vector<std::string_view>& arguments) {
    verbose = has_option(arguments, "-v");
    
    auto set_option_value = [&](const std::string_view& option, int& target) {
        if (auto value = get_option(arguments, option); !value.empty()) {
            try {
                target = std::stoi(std::string(value));
            } catch(const std::invalid_argument&) {
                throw std::invalid_argument(std::string("Invalid argument for option ") + std::string(option));
            } catch(const std::out_of_range&) {
                throw std::out_of_range(std::string("Out of range argument for option ") + std::string(option));
            }
        }
    };
    
    set_option_value("-s", sleep_length);
    set_option_value("-p", profile_time);
    set_option_value("-c", context_window);
    set_option_value("-i", heavy_profile_interval);
    num_threads = sysconf( _SC_NPROCESSORS_ONLN );
}

void process_values(std::vector<profiled_data>& data) {
    // Step 1: Normalize the capacity_adj values to have an average of 1.
    double sum = std::accumulate(data.begin(), data.end(), 0.0,
                                 [](double total, const profiled_data& pd) {
                                     return total + pd.capacity_adj;
                                 });
    double mean = sum / data.size();

    for (profiled_data& pd : data) {
        pd.capacity_adj /= mean;
    }

    // Step 2: Round capacity_adj to nearest multiple of 0.2
    for (profiled_data& pd : data) {
        pd.capacity_adj = std::round(pd.capacity_adj * 5) / 5;
    }
}


void getFinalizedData(int numthreads,double profile_time,std::vector<raw_data>& data_begin,std::vector<raw_data>& data_end,std::vector<profiled_data>& result_arr,std::vector<thread_args*> thread_arg){
  for (int i = 0; i < numthreads; i++) {
      u64 stolen_pass = data_end[i].steal_time - data_begin[i].steal_time;
      u64 preempts = data_end[i].preempts - data_begin[i].preempts;
      if(preempts == 0){
        std::cout<<"something went wrong"<<profile_time<<"Idk "<<data_begin[i].preempts<<" stl"<<data_begin[i].steal_time<<" aa"<<data_end[i].steal_time;
      }
      result_arr[i].capacity_perc = ((profile_time*1000000)-stolen_pass)/(profile_time*1000000);
      if (profiler_iter % heavy_profile_interval == 0){
        double perf_use = thread_arg[i]->user_time;
        std::cout<<"use time"<<perf_use;

        result_arr[i].capacity_adj = (1/perf_use) * data_end[i].raw_compute * 1/result_arr[i].capacity_perc;
        if(result_arr[i].capacity_adj<1000000){
          std::cout<<"LISTEN UP"<<data_end[i].raw_compute<<"WHAAAA "<<(1/perf_use)<<std::endl;
        }
      }

      result_arr[i].preempts = preempts;
      if(preempts == 0){
        if(stolen_pass != 0){
          std::cout<< "incompatible steal/preempt"<<std::endl;
          return;
        }
        result_arr[i].latency = 0;
      } else {
        result_arr[i].latency = stolen_pass/preempts; 
      }

      addToHistory(result_arr[i].capacity_perc_hist,result_arr[i].capacity_perc);
      addToHistory(result_arr[i].capacity_adj_hist,result_arr[i].capacity_adj);
      addToHistory(result_arr[i].latency_hist,result_arr[i].latency);
      addToHistory(result_arr[i].preempts_hist,result_arr[i].preempts);

      result_arr[i].capacity_perc_ema = calculate_ema(0.5,result_arr[i].capacity_perc_ema_a,result_arr[i].capacity_perc_ema,result_arr[i].capacity_perc);
      
      result_arr[i].capacity_perc_stddev = calculateStdDev(result_arr[i].capacity_perc_hist);
    };
    if (profiler_iter % heavy_profile_interval == 0){
        //process_values(result_arr);
      }
}

void printResult(int cpunum,std::vector<profiled_data>& result,std::vector<thread_args*> thread_arg){
  for (int i = 0; i < cpunum; i++){
        std::cout <<"CPU:"<<i<<" TID:"<<thread_arg[i]->tid<<std::endl;
        std::cout<<"Capacity Perc:"<<result[i].capacity_perc<<" Latency:"<<result[i].latency<<" Preempts:"<<result[i].preempts<<" Capacity Raw:"<<result[i].capacity_adj<<std::endl;
        std::cout<<"Cperc stddev:"<<result[i].capacity_perc_stddev;
        std::cout <<" Cperc ema: "<<result[i].capacity_perc_ema <<std::endl<<std::endl;
        
  }
  std::cout<<"--------------"<<std::endl;
}

void do_profile(std::vector<raw_data>& data_end,std::vector<thread_args*> thread_arg){

    std::vector<raw_data> data_begin(num_threads);
    std::vector<profiled_data> result_arr(num_threads);

    while(true){
      //sleep for sleep_length
      std::this_thread::sleep_for(std::chrono::milliseconds(sleep_length));

      //Set time where threads stop
      endtime = high_resolution_clock::now() + std::chrono::milliseconds(1000000000);
      if ((profiler_iter-1) % heavy_profile_interval == 0){
        for (int i = 0; i < num_threads; i++) {
          moveThreadtoLowPrio(thread_arg[i]->tid);
        }
      }

      //wake up threads and broadcast 
      initialized = 1;
      pthread_cond_broadcast(&cv);
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      endtime = high_resolution_clock::now() + std::chrono::milliseconds(profile_time);
      get_cpu_information(num_threads,data_begin,thread_arg);
      //Wait for processors to finish profiling
      //TODO-sleep every x ms and wake up to see if it's now(potentially)try nano sleep? (do some testing)

      std::this_thread::sleep_for(std::chrono::milliseconds(profile_time));
    
      get_cpu_information(num_threads,data_end,thread_arg);
	double test = (profile_time 
+ static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count())
- static_cast<double>(std::chrono::duration_cast<std::chrono::milliseconds>(endtime.time_since_epoch()).count()));

      getFinalizedData(num_threads,test,data_begin,data_end,result_arr,thread_arg);
      if ((profiler_iter+1) % heavy_profile_interval == 0){
        for (int i = 0; i < num_threads; i++) {
          moveThreadtoHighPrio(thread_arg[i]->tid);
        }
      }
      profiler_iter++;
      if(verbose){
        printResult(num_threads,result_arr,thread_arg);
      }
    }
}

std::vector<thread_args*> setup_threads(std::vector<pthread_t>& thread_array,std::vector<raw_data>& data_end){
  cpu_set_t cpuset;
  std::vector<thread_args*> threads_arg(num_threads);
  //create all the threads and initilize mutex
  for (int i = 0; i < num_threads; i++) {
    struct thread_args *args = new struct thread_args;
    //init mutex
    //TODO:use pthread_mutex_init
    //decide which cores to bind cpus too
    CPU_ZERO(&cpuset);
    CPU_SET(i, &cpuset);
    //give an id and assign mutex to all threads
    args->id = i;
    args->mutex = PTHREAD_MUTEX_INITIALIZER;
    args->addition_calc = &(data_end[i].raw_compute);
    //set prio of thread to MIN
    //TODO-error handling for thread creation mistakes
    pthread_create(&thread_array[i], NULL, run_computation, (void *) args);
    pthread_setaffinity_np(thread_array[i], sizeof(cpu_set_t), &cpuset);
    threads_arg[i] = args;
  }
  //we need to make sure that all the threads have fetched the thread ID before we go into whatever computation
  while(true){
    bool allset = true;
    for (int i = 0; i < num_threads; i++) {
      if(threads_arg[i]->tid == -1 ){
        bool allset = false;
      }
    }
    if(allset){
      break;
    }
   }
  
  return threads_arg;
}



int main(int argc, char *argv[]) {
  //the threads need to be moved to root level cgroup before they can be distributed to high/low cgroup
  moveCurrentThread();
  
  //Setting up arguments
  const std::vector<std::string_view> args(argv, argv + argc);
  setArguments(args);

  std::vector<pthread_t> thread_array(num_threads);
  //note that this needs to be here because the computations and the main thread need to communicate with each other
  std::vector<raw_data> data_end(num_threads);

  std::vector<thread_args*> threads_arg = setup_threads(thread_array,data_end);
   moveThreadtoHighPrio(syscall(SYS_gettid));



  do_profile(data_end,threads_arg);

  //TODO-Close or start on command;
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

u64 timespec_diff_to_ns(struct timespec *start, struct timespec *end) {
    u64 start_ns = start->tv_sec * 1000000000LL + start->tv_nsec;
    u64 end_ns = end->tv_sec * 1000000000LL + end->tv_nsec;
    return end_ns - start_ns;
}

void* run_computation(void * arg)
{
    //TODO-Learn how to use kernel shark to visualize whole process
    struct thread_args *args = (struct thread_args *)arg;
    moveThreadtoLowPrio(syscall(SYS_gettid));
    args->tid = syscall(SYS_gettid);
    while(true) {
      struct timespec start,end;
      //here to avoid a race condition
      bool heavy_interval = false;
      bool test_interval = false;
      pthread_mutex_lock(&args->mutex);
      while (! initialized) {
        pthread_cond_wait(&cv, &args->mutex);
      }
      pthread_mutex_unlock(&args->mutex);
      
      int addition_calculator = 0;
      if (profiler_iter % heavy_profile_interval == 0){
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &start);
        heavy_interval = true;
      }
      while(std::chrono::high_resolution_clock::now() < endtime) {
        addition_calculator += 1;
      };
      *args->addition_calc = addition_calculator;
      if(heavy_interval){
        clock_gettime(CLOCK_THREAD_CPUTIME_ID, &end);
        double test = static_cast<double>(timespec_diff_to_ns(&start, &end)) /
(profile_time * 1e6 
+ static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count()) 
- static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(endtime.time_since_epoch()).count()));
        if(test>1){
          std::cout<<"SOMETHING IS WRONG"<< timespec_diff_to_ns(&start, &end) <<" WHAT"<<(profile_time * 1e6 
+ static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count()) 
- static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(endtime.time_since_epoch()).count()))<<std::endl;
        }
        args->user_time = test;
        }
      
      initialized = 0;
      }
      return NULL;
} 
