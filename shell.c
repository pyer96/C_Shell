/*################################_Pier_Luigi_Manfrini_###############################
 *
 *	 This is a simple mini shell project!
 *
 *	 Usage:
 *	 	<./shell>
 *
 *	 Functionalities:
 *	 	- Basic Program Execution
 *
 *		- History function pressing Up and Down Arrows (non blocking mode
 *			for the input has been necessary)
 *
 *		- A small list of built-in commands:
 *			> exit
 *			> cd
 *			> pwd
 *			> clear
 *
 *		- Dynamical Promote String with the current working directory
 *			and green arrow (~>) for correct previous return
 *			status and red arrow for failed execution
 *		  	PS = mySH:<cwd> ~>
 *
 *	 	- STDOUT and STDERR for each called process is also logged
 *	 		into a log file named "<proc_name>_pid<proc_pid>.log"
 *	 		and placed inside the folder mySH_logs
 *	
 *	Compile:
 *		gcc shell.c -o shell -Wall -Werror -fsanitize=leak -lncurses
 *
 * ###################################################################################
 */


#include "shell.h"

void exec_child(_shell_Descriptor *);
char *read_line(_shell_Descriptor *);
void free_all(children *);
void free_last(_shell_Descriptor*);
children *add_child_to_list(_shell_Descriptor *, char **);
char **parse_string(char *);
void init_shell(_shell_Descriptor *);
int is_special(char *);
void change_dir(_shell_Descriptor *);
void init_ncurses(void);
void print_promote_string(_shell_Descriptor *);
void logger(_shell_Descriptor *, pid_t);

int main() {
   init_ncurses();
  _shell_Descriptor *shell =
      (_shell_Descriptor *)malloc(sizeof(_shell_Descriptor));
  init_shell(shell);
  while (shell->active) {
    pid_t pid;
    /* Printing the promote string */
    print_promote_string(shell);

    /* Read And Parse Input */
    shell->ChildrenHistoryLast =
        add_child_to_list(shell, parse_string(read_line(shell)));

    /* If just Enter pressed */
    if (!strcmp(shell->ChildrenHistoryLast->command[0],"\0")) {
      shell->last_status = true;
      free_last(shell);
      continue;
    }

    /* Check for Special Commands */
    switch (is_special(shell->ChildrenHistoryLast->command[0])) {
    case -1:
      break; // Not a Special Command

    case 0: { // EXIT
      shell->active = false;
      continue;
      break;
    }
    case 1: { // CD
      change_dir(shell);
      continue;
      break;
    }
    case 2: { // PWD
      dprintf(1, "%s", shell->curr_Dir);
      printw("\n  \r");
      refresh();
      shell->last_status = true;
      continue;
      break;
    }
    case 3:  // Clear
      erase();
      mvwin(stdscr,0,0);
      break;	    
    }// end Switch for special commands

    /* Fork: If no special command found fork is executed */
    pipe(shell->child2shell);
    switch (pid = fork()) {
    case -1: {
      perror("Error while forking!\n(PERROR)");
      break;
    }
    case 0: { /* Child Process */
      exec_child(shell);
      break;
    }
    default: { /*  ~Shell~ */
      int status;
	logger(shell, pid);
      if (waitpid(pid, &status, 0) == -1)
        perror("Error in waiting a child process!\n(PERROR)");
      if (status == 0){
        shell->last_status = true; }// child returned correctly
      else
        shell->last_status = false; // child returned with errors
      break;
    }
    }
  } /* end While(shell->active=false) */

  if (shell->ChildrenHistory != NULL)
    free_all(shell->ChildrenHistory);
  free(shell->_hostname); // gethostname does malloc
  free(shell);
  endwin();
  return 0;
}
