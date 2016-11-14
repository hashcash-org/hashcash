/* hashfork.c
 *
 * by Hubert Chan
 *
 * This file is hereby placed in the public domain.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>

const int NUM_PROCS=15;

int main (int argc, char *argv[])
{
  int i;
  pid_t children[NUM_PROCS];
  pid_t pid;
  int status;

  for (i = 0; i < NUM_PROCS; i++)
  {
    pid = fork ();
    if (pid)
    { /* parent */
      children[i] = pid;
    }
    else
    {
      int wait_time = 0;
      pid = getpid ();
      printf ("process %d started.\n", pid);
      /* this should be a function call to calculate the hashcash instead of
       * an execl */
      execl ("/usr/bin/hashcash", "/usr/bin/hashcash", "-mb25", "foo", NULL);
      exit(0);

      /* --- ignore this --- */
      srand (time (NULL) + pid);
      wait_time = rand () % 20;
      printf ("%d: waiting %d seconds.\n", pid, wait_time);
      sleep (wait_time);
      printf ("%d: exiting.\n", pid);
      exit (0);
    }
  }

  pid = wait (&status);

  printf("process %d exited.\nkilling all children.\n", pid);

  for (i = 0; i < NUM_PROCS; i++)
  {
    printf ("%d\n", children[i]);
    kill (children[i], SIGKILL);
  }

  return 0;
}
