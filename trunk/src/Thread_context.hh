/*
    This file is part of GNU APL, a free implementation of the
    ISO/IEC Standard 13751, "Programming Language APL, Extended"

    Copyright © 2008-2026  Dr. Jürgen Sauermann

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/** @file
*/

#ifndef __THREAD_CONTEXT_HH_DEFINED__
#define __THREAD_CONTEXT_HH_DEFINED__

#include <pthread.h>

#include "PJob.hh"

//════════════════════════════════════════════════════════════════════════════
/**
  Multi-core GNU APL uses a pool of threads numbered 0, 1, ... core_count()-1

  (master-) thread 0 is the interpreter (which also performs parallel work),
  while (worker-) threads 1 ... are only activated when some parallel work
  is available.

  The worker threads 1... are either working, or busy-waiting for work, or
  blocked on a semaphore:

         init
          ↓
       blocked ←→ busy-waiting ←→ working

  The transitions

      blocked ←→ busy-waiting

  occur before and after the master thread waits for terminal input or when
  the number of cores is being changed (with ⎕SYL[26;2]). The workers are
  blocked while the master thread waits for terminal input. The reason for this
  is that a busy-waiting worker is consuming power for no reason (to the extent
  that the CPU fan turns on even though there is no useful work in sight).

  The transitions

        busy-waiting ←→ working

  occurs when the execution of APL primitives, primarily scalar functions,
  suggests to execute in parallel (i.e. if the vectors involved are
  sufficiently long).

 **/
//════════════════════════════════════════════════════════════════════════════
/// the context for one parallel execution thread
class Thread_context
{
public:
   /// constructor
   Thread_context();

   /// destructor
   ~Thread_context();

   /// a function that is executed by the pool (i.e. in parallel)
   typedef void PoolFunction(Thread_context & ctx);

   /// return the number of the core executing \b this context
   CoreNumber get_N() const
       { return N; }

   /// start parallel execution of work in a worker
   void PF_fork()
      {
        while (get_master().job_number == job_number)
              /* busy wait until the master has increased job_number */ ;
      }

   /// end parallel execution of work in a worker
   void PF_join()
      {
        atomic_add(busy_worker_count, -1);   // we are ready
        ++job_number;            // we reached master job_number

        // wait until all workers finished or new job from master
        while (atomic_read(busy_worker_count) != 0 &&
               get_master().job_number == job_number)
              /* busy wait */ ;
      }

   /// cancel all dyadic jobs in all thread contexts
   static void cancel_all_dyadic_jobs()
      {
        loop(a, get_active_core_count())
             get_context(CoreNumber(a))->joblist_AB.cancel_jobs();
      }

   /// cancel all monadic jobs in all thread contexts
   static void cancel_all_monadic_jobs()
      {
        loop(a, get_active_core_count())
             get_context(CoreNumber(a))->joblist_B.cancel_jobs();
      }

   /// number of currently used cores
   static CoreCount get_active_core_count()
      { return active_core_count; }

   /// return the context for core \b n
   /// @param n core number whose context to retrieve
   static Thread_context * get_context(CoreNumber n)
      { return thread_contexts + n; }

   /// return the context of the master
   static Thread_context & get_master()
      { return thread_contexts[CNUM_MASTER]; }

   /// start parallel execution of work at the master
   /// @param jname descriptive name of the parallel job being started
   static void M_fork(const char * jname)
      {
        get_master().job_name = jname;
        atomic_add(busy_worker_count, active_core_count - 1);
        ++get_master().job_number;
      }

   /// end parallel execution of work at the master
   static void M_join()
      {
        while (atomic_read(busy_worker_count) != 0)   /* busy wait */ ;
      }

   /// bind thread to core
   /// @param cpu CPU core number to bind this thread to
   /// @param logit true to log the binding operation
   void bind_to_cpu(CPU_Number cpu, bool logit);

   /// make all workers lock on pool_sema
   void M_lock_pool();

   /// print this TaskTree node
   /// @param out output stream to write to
   void print(ostream & out) const;

   /// remove all thread contexts (when the APL interpreter exits)
   static void cleanup();

   /// initialize all thread contexts (set all but N and thread)
   /// @param thread_count number of parallel worker threads to create
   /// @param logit true to log initialization details
   static void init_parallel(CoreCount thread_count, bool logit);

   /// initialize all thread contexts (set all but N and thread)
   /// @param logit true to log initialization details
   static void init_sequential(bool logit);

   /// terminate all worker threads
   static void kill_pool();

   /// print all TaskTree nodes
   /// @param out output stream to write to
   static void print_all(ostream & out);

   /// print all mileages nodes
   /// @param out output stream to write to
   /// @param loc caller location for diagnostics
   static void print_mileages(ostream & out, const char * loc);

   /// set the number of currently used cores
   /// @param new_count desired number of active cores
   static void set_active_core_count(CoreCount new_count);

   /// the thread executing this context
   pthread_t thread;

   /// a counter controlling the start of parallel jobs
   volatile char job_number;

   /// return the name of this context
   const char * job_name;

   /// true if this context shall join (i.e. is not the master)
   bool do_join;

   /// a semaphore to block this context
   sem_t pool_sema;

   /// true if blocked on pool_sema
   bool blocked;

   /// a list of monadic PJobs created by this core
   Parallel_job_list<PJob_scalar_B, false> joblist_B;

   /// a list of dyadic PJobs created by this core
   Parallel_job_list<PJob_scalar_AB, false> joblist_AB;

   /// the next work to be done
   static PoolFunction * do_work;

   /// a thread-function that should not be called
   static PoolFunction PF_no_work;

   /// block/unblock on the pool semaphore
   static PoolFunction PF_lock_unlock_pool;

protected:
   /// initialize thread_contexts[n]
   /// @param n index of the thread context entry to initialize
   void init_entry(CoreNumber n);

   /// thread number (0 = interpreter, 1... worker threads
   CoreNumber N;

   /// the cpu core to which this thread is bound
   CPU_Number CPU;

   /// all thread contexts
   static Thread_context * thread_contexts;

   /// the number of thread contexts
   static CoreCount thread_contexts_count;

   /// the number of unfinished workers (excluding master)
   static volatile _Atomic_word busy_worker_count;

   /// the number of cores currently used
   static CoreCount active_core_count;
};
//════════════════════════════════════════════════════════════════════════════

#endif // __THREAD_CONTEXT_HH_DEFINED__
