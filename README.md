# C_Shell	
Simple C shell which also keep a log of all commands executed

  To keep it simple, everything has been mantained organized only into
	2 files (shell.c and shell.h) leaving the .c as clean as possible.

	The shell's characteristics are described withing the source file
	shell.c.
	However there are still present some minor issues that up to today I did
	not fixed:
		- When displaying dynamical outputs produced by programs
			like TOP or HTOP after some refreshes the output looses
			unexpectedly its proper formatting
		- Right after failures due to dynamical outputs the commands may be
			not properly received (only for the command right after the failure)


