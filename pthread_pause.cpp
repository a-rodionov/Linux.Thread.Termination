
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
std::atomic_bool isStopPhase1( false );
std::atomic_bool isStopPhase2( false );


void SuspendHandler(int /*signal*/)
{
  std::cout << "SuspendHandler\n";

  int pendingSignal = 0;
  sigset_t set;

  if(sigemptyset( &set ))
  {
    anyError = true;
    return;
  }
  if(sigaddset(&set, RESUME_SIGNAL))
  {
    anyError = true;
    return;
  }

  if(sigwait(&set, &pendingSignal))
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

bool MakeThreadSuspendable()
{
  sigset_t mask;

  if(sigemptyset(&mask))
  {
   return false;
  }
  if(sigaddset(&mask, SUSPEND_SIGNAL))
  {
    return false;
  }
  if(sigaddset(&mask, RESUME_SIGNAL))
  {
    return false;
  }
  auto error = pthread_sigmask(SIG_UNBLOCK, &mask, nullptr);
  if(error)
  {
   return false;
  }

  struct sigaction act;
  memset(&act, 0, sizeof(act));

  sigset_t set;
  if(sigfillset(&set))
  {
    return false;
  }
  act.sa_mask = set;

  act.sa_handler = SuspendHandler;
  if(sigaction(SUSPEND_SIGNAL, &act, 0))
  {
    return false;
  }

  act.sa_handler = SIG_IGN;
  if(sigaction(RESUME_SIGNAL, &act, 0))
  {
    return false;
  }
  return true;
}

bool MuteSuspendSignal(struct sigaction* oldact)
{
  struct sigaction act;
  memset( &act, 0, sizeof( act ));

  sigset_t set;
  if( sigfillset( &set ) )
  {
    return false;
  }
  act.sa_mask = set;

  act.sa_handler = SIG_IGN;
  if(sigaction( SUSPEND_SIGNAL, &act, oldact ))
  {
    return false;
  }
  return true;
}

bool UnmuteSuspendSignal(struct sigaction* oldact)
{
  if(sigaction(SUSPEND_SIGNAL, oldact, nullptr))
  {
    return false;
  }
  return true;
}

void ThreadFunction()
{
  struct sigaction oldact;
  if(!MuteSuspendSignal( &oldact))
  {
    std::cout << "Failed to mute suspend signal\n";
  }
  std::cout << "Thread muted suspend signal\n";

  ready = true;
  cv.notify_one();

  while(!anyError && !isStopPhase1)
  {
    std::cout << '.';
  }

  if(!UnmuteSuspendSignal(&oldact))
  {
    std::cout << "Failed to unmute suspend signal\n";
  }
  std::cout << "Thread unmuted suspend signal\n";

  while(!anyError && !isStopPhase2)
  {
    std::cout << '.';
  }
}


int main()
{
  if(!MakeThreadSuspendable())
  {
    std::cout << "Failed to set signal handler." << std::endl;
    return 0;
  }

  std::thread thread(ThreadFunction);
  {
    std::unique_lock<std::mutex> lk(m);
    cv.wait(lk, [] { return ready; });
  }

  usleep(50);
  std::cout << "main sends resume signal\n";

  if(pthread_kill(thread.native_handle(), RESUME_SIGNAL))
  {
    std::cout << "Failed to send RESUME_SIGNAL." << std::endl;
    return 0;
  }

  usleep(50);
  std::cout << "main sends suspend signal\n";

  if(pthread_kill(thread.native_handle(), SUSPEND_SIGNAL))
  {
    std::cout << "Failed to send SUSPEND_SIGNAL." << std::endl;
    return 0;
  }

  usleep(50);
  std::cout << "main sends resume signal\n";

  if(pthread_kill(thread.native_handle(), RESUME_SIGNAL))
  {
    std::cout << "Failed to send RESUME_SIGNAL." << std::endl;
    return 0;
  }

  std::cout << "main sets stopPhase1 flag\n";
  isStopPhase1 = true;

  usleep(50);
  std::cout << "main sends suspend signal\n";

  if(pthread_kill(thread.native_handle(), SUSPEND_SIGNAL))
  {
    std::cout << "Failed to send SUSPEND_SIGNAL." << std::endl;
    return 0;
  }

  usleep(50);
  std::cout << "main sends resume signal\n";

  if(pthread_kill(thread.native_handle(), RESUME_SIGNAL))
  {
    std::cout << "Failed to send RESUME_SIGNAL." << std::endl;
    return 0;
  }

  usleep(50);
  std::cout << "main sets stopPhase2 flag\n";
  isStopPhase2 = true;

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