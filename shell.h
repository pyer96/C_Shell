#include <curses.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_FSYS_NAMES 256
#define READ 0
#define WRITE 1

static size_t read_block = 256;

typedef struct children {
  char **command;
  struct children *prev;
  struct children *next;
} children;

typedef struct _shell_Descriptor {
  bool active;
  int max_x, max_y;
  int ps_x, ps_y;
  char *_hostname;
  uid_t _usr;
  pid_t _shell_pid;
  int child2shell[2];
  bool last_status;
  char *curr_Dir;
  char logs_Dir[MAX_FSYS_NAMES];
  // char **command;
  children *ChildrenHistory;
  children *ChildrenHistoryLast;
} _shell_Descriptor;

const char *const special_commands[5] = {"exit", "cd", "pwd", "clear", NULL};

// ***********************INIT NCURSES***********************
void init_ncurses(void) {
  initscr();
  cbreak();
  keypad(stdscr, TRUE);
  scrollok(stdscr, TRUE);
  noecho();
  if (has_colors() == FALSE) {
    endwin();
    printf("Your terminal does not support color!\n");
    exit(EXIT_FAILURE);
  }
  use_default_colors();
  start_color();
  init_pair(1, COLOR_WHITE, -1);
  init_pair(2, COLOR_YELLOW, -1);
  init_pair(3, COLOR_RED, -1);
  init_pair(4, COLOR_GREEN, -1);
  init_pair(5, COLOR_MAGENTA, -1);
}

// **************************INIT SHELL**********************
void init_shell(_shell_Descriptor *shell) {
  setenv("MYSH_ROOT", getenv("PWD"),
         0); 	// if run with sudo it will yeld SEGM FAULT since no PWD 
 		// entry is present in the root's env
  if (sprintf(shell->logs_Dir, "%s/mySH_logs", getenv("MYSH_ROOT")) < 0)
    dprintf(2, "Error while generating the string name for the log_Dir!\n");
  struct stat st;
  if (stat(shell->logs_Dir, &st)) { // && S_ISDIR(st.st_mode)) {
    /* if directory doesnt already exist, it gets created */
    if (mkdir(shell->logs_Dir, 0777))
      perror("mkdir");
  }
  shell->active = true;
  shell->curr_Dir = getenv("PWD");
  shell->_hostname = (char *)malloc(MAX_FSYS_NAMES * sizeof(char));
  if (gethostname(shell->_hostname, MAX_FSYS_NAMES - 1) ==
      -1) // need to leave space for '\0' (MAX_FSYS_NAMES -1)
    perror("");
  shell->_usr = getuid();
  shell->_shell_pid = getpid();
  shell->ChildrenHistory = NULL;
  shell->ChildrenHistoryLast = NULL;
  // shell->command = NULL;
  shell->last_status = true;
  getmaxyx(stdscr, shell->max_y, shell->max_x);
}

// ***********************IS SPECIAL**************************
int is_special(char *command) {
  int i;
  for (i = 0; special_commands[i] != NULL; i++) {
    if (!strcmp(command, special_commands[i]))
      return i;
  }
  return -1;
}

// **************************CHANGE DIR************************
void change_dir(_shell_Descriptor *shell) {
  if (shell->ChildrenHistoryLast->command[1] == NULL) {
    if (chdir(getenv("HOME")) == -1) {
      perror("chdir");
      shell->last_status = false;
    }
  } else {
    if (chdir(shell->ChildrenHistoryLast->command[1]) == -1) {
      printw("Invalid Directory!\n");
      shell->last_status = false;
    }
    shell->last_status = true;
  }

  if (!getcwd(shell->curr_Dir, MAX_FSYS_NAMES)) {
    printw("Error while getting current directory!\n");
    refresh();
  }
}

// ***********************ADD CHILD TO LIST ************************
children *add_child_to_list(_shell_Descriptor *shell, char **command) {

  if (shell->ChildrenHistory == NULL) {
    shell->ChildrenHistory = (children *)malloc(sizeof(children));
    (shell->ChildrenHistory)->command = command;
    (shell->ChildrenHistory)->next = NULL;
    (shell->ChildrenHistory)->prev = NULL;
    return shell->ChildrenHistory;
  } else {
    children *p = shell->ChildrenHistory;
    while (p->next != NULL) {
      p = p->next;
    }
    p->next = (children *)malloc(sizeof(children));
    p->next->command = command;
    p->next->next = NULL;
    p->next->prev = p;
    return p->next;
  }
}

// ********************PRINT PROMOTE STRING******************
void print_promote_string(_shell_Descriptor *shell) {
  move(getcury(stdscr), 0);
  attron(COLOR_PAIR(2) | A_BOLD);
  printw("mySH");
  addch(':' | COLOR_PAIR(1) | A_BOLD);
  attron(COLOR_PAIR(3) | A_BOLD);
  printw("%s", shell->curr_Dir);
  if (shell->last_status)
    attron(COLOR_PAIR(4) | A_BOLD);
  else
    attron(COLOR_PAIR(3) | A_BOLD);
  printw(" ~> ");
  attroff(A_BOLD);
  attron(COLOR_PAIR(4));
  refresh();
  getyx(stdscr, shell->ps_y, shell->ps_x);
//  printw("X:%d Y:%d", shell->ps_x, shell->ps_y);
  refresh();
}

// *********************FREE COMMAND****************************
void free_cmd(char **cmd) {
  int i = 0;
  do {
    free(cmd[i]);
    i++;
  } while (cmd[i] != NULL);
}

// **********************FREE LAST****************************
void free_last(_shell_Descriptor *shell) {
  if (shell->ChildrenHistoryLast == shell->ChildrenHistory) {
    free_cmd(shell->ChildrenHistoryLast->command);
    free(shell->ChildrenHistoryLast->command);
    free(shell->ChildrenHistoryLast);
    shell->ChildrenHistory = NULL;
  } else {
    shell->ChildrenHistoryLast->prev->next = NULL;
    free_cmd(shell->ChildrenHistoryLast->command);
    free(shell->ChildrenHistoryLast->command);
    free(shell->ChildrenHistoryLast);
    shell->ChildrenHistoryLast = shell->ChildrenHistoryLast->prev;
  }
}

// **********************FREE ALL**************************
void free_all(children *list) {
  while (list->next != NULL) {

    children *tmp = list->next;
    free_cmd(list->command);
    free(list->command);
    free(list);
    list = tmp;
  }

  free_cmd(list->command);
  free(list->command);
  free(list);
}

// ***********************EXEC CHILD************************
void exec_child(_shell_Descriptor *shell) {
  close(shell->child2shell[READ]);

  /* Send STDOUT and STDERR to the logger */
  dup2(shell->child2shell[WRITE], 1);
  dup2(shell->child2shell[WRITE], 2);
  close(shell->child2shell[WRITE]);
  if (execvp(shell->ChildrenHistoryLast->command[0],
             shell->ChildrenHistoryLast->command) == -1)
    perror("");
  exit(EXIT_FAILURE);
}

// ********************* LOGGER *************************
void logger(_shell_Descriptor *shell, pid_t pid) {
  close(shell->child2shell[WRITE]);
  int fd;
  char log[2 * MAX_FSYS_NAMES];
  char buffer[4096];
  if (sprintf(log, "%s/%spid_%d.log", shell->logs_Dir,
              shell->ChildrenHistoryLast->command[0], pid) < 0)
    dprintf(2, "Error while generating the string name of the log file!\n");
  if ((fd = open(log, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1)
    perror("Error while opening the log file!\n(PERROR)");
  size_t num_read;
  while ((num_read = read(shell->child2shell[READ], buffer, 4096)) != 0) {
    ssize_t num_written;
    size_t total_written;
    // Write to STDOUT //
    total_written = 0;
    while (total_written < num_read) {
      if (buffer[total_written] == '\n') {
	      addch('\n');
	      dprintf(1,"\r");
//	      clrtoeol();
        //printw("  \n\r");
        refresh();
        total_written++;
      } else {
        dprintf(1, "%c", buffer[total_written++]);
        //	      addch(buffer[total_written++]);
        refresh();
      }
    }
    // Write to LOG //
    total_written = 0;
    while (total_written < num_read) {
      num_written = write(fd, buffer + total_written, num_read - total_written);
      total_written += num_written;
    }
  }
}

// *************************PARSE STRING*************************
char **parse_string(char *string) {
  char **command = (char **)malloc((strlen(string) / 2) * sizeof(char *));
  char *token = strtok(string, " ");
//  printw("DBG string[0]:%c\n",string[0]);
//   if (token==NULL) printw("DBG token:%s\n",token);
  if (token == NULL) {
    command[0] = (char *)malloc(2);
    strcpy(command[0], "\0");
    command[1] = NULL;
  } else {
    int i = 1;
    command[0] = (char *)malloc(strlen(token) + 1 * sizeof(char));
    strcpy(command[0], token);
    while ((token = strtok(NULL, " ")) != NULL) {
      command[i] = (char *)malloc(strlen(token) + 1 * sizeof(char));
      strcpy(command[i], token);
      i++;
    }
    command[i] = NULL;

    // DBG print all commands
    /*
        for (int i = 0; command[i] != NULL; i++) {
          printw("<%s ", command[i]);
          refresh();
        }
        printw(">");
        refresh();*/
  }
  free(string); // FREEE GAME CHANGER  -> influenced FREE ALL FREE CMD AND
                // FREE LAST
  return command;
}

// *************************BUILD BACK STRING FROM COMMAND**********************
char *build_back_str_from_command(char **command) {
  int i = 0;
  char *string = (char *)malloc(read_block * sizeof(char));
  while (command[i] != NULL) {
    strcat(string, command[i++]);
    if (command[i] != NULL)
      strcat(string, " ");
    else
      strcat(string, "\0");
  }
  return string;
}

// ****************************READ LINE***********************
char *read_line(_shell_Descriptor *shell) {
 //  printw("DBG enter READ_LINE");
  refresh();
  int x, y;
  char *input = (char *)malloc(read_block * sizeof(char));
  children *tmp_child;
  if (shell->ChildrenHistoryLast != NULL)
    tmp_child = shell->ChildrenHistoryLast;
  else if (shell->ChildrenHistoryLast == NULL)
    tmp_child = NULL;
  size_t i = 0;
  int ch;
  while ((ch = getch()) != EOF && ch != '\n') {

    // printw("WHILENTER");
    refresh();
    if (i == read_block - 2) {
      read_block += read_block;
      input = (char *)realloc(input, read_block);
    }
    if (ch == KEY_UP) { // KEY UP
      if (tmp_child == NULL) {
        continue;
      }
      char *tmp;
      getyx(stdscr, y, x);
      if(x==shell->ps_x){
 	if(shell->ChildrenHistoryLast != NULL)
		tmp_child = shell->ChildrenHistoryLast;
      }
      move(y, shell->ps_x);
      clrtoeol();
      tmp = build_back_str_from_command(tmp_child->command);
      strcpy(input, tmp);
      free(tmp);
      printw("%s", input);
      refresh();
      getyx(stdscr, y, x);
      i = x - shell->ps_x;
      if (tmp_child->prev != NULL)
        tmp_child = tmp_child->prev;
      continue;
    }
    if (ch == KEY_DOWN) { // KEY DOWN
      char *tmp;
      getyx(stdscr, y, x);
      if (x == shell->ps_x || tmp_child == NULL ||
          tmp_child == shell->ChildrenHistoryLast) {
	      move(shell->ps_y , shell->ps_x);
	      clrtoeol();
	      i=0;
        continue;
      } else {
        if (tmp_child->next != NULL)
          tmp_child = tmp_child->next;
        move(y, shell->ps_x);
        clrtoeol();
        tmp = build_back_str_from_command(tmp_child->command);
        strcpy(input, tmp);
        free(tmp);
        printw("%s", input);
        refresh();
        getyx(stdscr, y, x);
        i = x - shell->ps_x;
      }
      continue;
    } else if (ch == KEY_LEFT) { // KEY LEFT
      getyx(stdscr, y, x);
      if (x == shell->ps_x)
        continue;
      move(y, x - 1);
      refresh();
      i--;
      continue;
    } else if (ch == KEY_RIGHT) { // KEY RIGHT
      getyx(stdscr, y, x);
      if (x < shell->max_x) {
        move(y, x + 1);
        i++;
        refresh();
      }
      continue;
    } else if (ch == KEY_BACKSPACE || ch == KEY_DC ||
               ch == 127) { // BackSpace/Delete
      getyx(stdscr, y, x);
      if (x > shell->ps_x) {
        i--;
        move(y, x - 1);
        delch();
        refresh();
      }
      continue;
    } else if ((ch & A_CHARTEXT) == '\t') {
      input[i] = ' ';
      getyx(stdscr, y, x);
      move(y, shell->ps_x + i + 1);
      refresh();
    } else { // Normal character
      input[i] = (char)ch & A_CHARTEXT;
      addch(input[i]);
      refresh();
    }

    i++;
      // printw("END READ LINE WHILE");
  }

 			 // Enter Key pressed
  // printw("ENTER PRESSED");
  addch('\n');
  //  addch('\r');
  refresh();
  clrtoeol();
  refresh();
  input[i] = ' ';
  input[i + 1] = '\0';
  //printw("DBG input from READ_LINE: <%s>", input);
  refresh();
  return input;
}
