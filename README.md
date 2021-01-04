# posix_studying_lab
I'm learning concepts of low-level UNIX programming, these files are my experimental sandbox.

##termios_snake
I tested some advanced terminal IO, so I wrote a simple snake game.
I doesn't use any kind of curses library, only termios and ANSI escape sequences.

Known issues with the snake:
1) The code has been prototyped "as is" while testing out the new features, has to be refactored.
2) Each "frame" must be buffered and processed with a single write() syscall instead of multiple.
3) No error handling, but it _must_ be added.

## next
I'm studying some p_socket programming, and I think I will come up with something here as well.
