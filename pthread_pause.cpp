#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <signal.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

static const int RESUME_SIGNAL = SIGUSR1;
static const int SUSPEND_SIGNAL = SIGUSR2;

std::mutex m;
std::condition_variable cv;
bool ready = false;
const static int sleep_period_sec = 1;
std::atomic_bool wasPaused( false );
std::atomic_bool anyError( false );

void SuspendHandler( int /*signal*/ )
{
  int pendingSignal = 0;  
  sigset_t set;

  if(sigemptyset( &set ))
  {
    anyError = true;
    return;
  }
  if(sigaddset( &set, RESUME_SIGNAL ))
  {
    anyError = true;
    return;
  }

  if(sigwait( &set, &pendingSignal ))
  {
    anyError = true;
    return;
  }

  if(RESUME_SIGNAL != pendingSignal)
  {
    anyError = true;
    return;
  }

  wasPaused = true;
}

bool SetSignalHandler()
{
  struct sigaction act;
  memset( &act, 0, sizeof( act ));

  sigset_t set;
  if(sigemptyset( &set ))
  {
    return false;
  }
  if(sigaddset( &set, RESUME_SIGNAL ))
  {
    return false;
  }
  if(sigaddset( &set, SUSPEND_SIGNAL ))
  {
    return false;
  }
  act.sa_mask = set;

  act.sa_handler = SuspendHandler;
  if(sigaction( SUSPEND_SIGNAL, &act, 0 ))
  {
    return false;
  }
  return true;
}

void ThreadFunction()
{
  ready = true;
  cv.notify_one();

  while(!wasPaused || anyError)
  {
    sleep(sleep_period_sec);
  }
}

int main()
{
  if(!SetSignalHandler())
  {
    std::cout << "Failed to set signal handler." << std::endl;
    return 0;
  }

  std::thread thread(ThreadFunction);
  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return ready; });
  }

  if(pthread_kill(thread.native_handle(), SUSPEND_SIGNAL))
  {
    std::cout << "Failed to send SUSPEND_SIGNAL." << std::endl;
    return 0;
  }
  sleep(sleep_period_sec);
  if(pthread_kill(thread.native_handle(), RESUME_SIGNAL))
  {
    std::cout << "Failed to send RESUME_SIGNAL." << std::endl;
    return 0;
  }

  if(!thread.joinable())
  {
    std::cout << "Fatal error. Thread isn't joinable." << std::endl;
    return 0;
  }

  thread.join();

  if(anyError)
  {
    std::cout << "Some error occured in signal handler." << std::endl;
  }

  if(wasPaused)
  {
    std::cout << "Thread was successfully paused." << std::endl;
  }
  else
  {
    std::cout << "Thread wasn't paused." << std::endl;
  }

  return 0;
}