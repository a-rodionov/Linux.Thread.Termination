#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <map>
#include <signal.h>
#include <unistd.h>
#include <string.h>

std::mutex m;
std::condition_variable cv;
bool ready = false;
bool processed = false;
const static int sleep_period_sec = 5;

void mask_sig(void)
{
  sigset_t mask;
  sigemptyset(&mask); 
  sigaddset(&mask, SIGRTMIN+3); 
  pthread_sigmask(SIG_BLOCK, &mask, NULL);
}

void sighandler(int signo)
{ 
  return; 
}

void set_sig_handler(void)
{
  struct sigaction action;
  
  memset(&action, 0, sizeof(action));
  action.sa_handler = sighandler;
  
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGRTMIN + 3);
  action.sa_mask = set;

  if (sigaction(SIGRTMIN + 3, &action, NULL) == -1)
  {
    _exit(1);
  }
}

void f_sleep()
{
  ready = true;
  cv.notify_one();
    
  sleep(sleep_period_sec);
    
  processed = true;
  cv.notify_one();
}

void f_usleep()
{
  ready = true;
  cv.notify_one();
    
  usleep(sleep_period_sec * 1000);
    
  processed = true;
  cv.notify_one();
}

void f_sleep_for()
{
  ready = true;
  cv.notify_one();
    
  std::this_thread::sleep_for(std::chrono::seconds(sleep_period_sec));
    
  processed = true;
  cv.notify_one();
}

void run(void(*threadFunction)(), const std::string& functionName)
{
  ready = false;
  processed = false;

  std::thread thread_inst(threadFunction);
  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return ready; });
  }
  auto start = std::chrono::system_clock::now();

  pthread_kill(thread_inst.native_handle(), SIGRTMIN + 3);

  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return processed; });
  }
  if(thread_inst.joinable())
  {
    thread_inst.join();
  }
  auto end = std::chrono::system_clock::now();

  std::cout << "Thread was going to sleep for " << sleep_period_sec << " seconds using " << functionName 
            << " function, but tried to be interrupted by signal and executed for " << std::chrono::duration<double>(end-start).count()
            << " seconds." << std::endl;
}
 
int main()
{
  const std::map<void(*)(), const std::string> functions {{&f_sleep, "sleep"},
                                                          {&f_usleep, "usleep"},
                                                          {&f_sleep_for, "sleep_for"}};
  set_sig_handler();
  //mask_sig();
  for(const auto& function : functions)
  {
    run(function.first, function.second);
  }
  return 0;
}
