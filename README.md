# Helio-Text-Editor

Helio Text Editor is a simple text editor designed for quick and simple use. It is designed with a minimalistic approach and is built in the C langauge using the C99 standard. It runs on Unix terminals with a specific focus on the Ubuntu terminal environment; I personally used WSL2 to test and run my program.

## Features

- Open Files
- Type Text
- Move Cursor and Scroll
- Save Files
- Status Bar and Help Bar

## Getting Started

To build and run the Helio Text Editor, you need a C compiler and the basic build tools. You'll also need to set up a Linux environment within Windows since 
the text editor interacts with the terminal using the `<termios.h>` header, which isn’t available on Windows. Here are the suggested methods:

### Windows (WSL2 Installation)

1. **Install Windows Subsystem for Linux 2 (WSL2):**
   - You can conveniently install all the prerequisites for WSL with a single command. Open PowerShell or Windows Command Prompt in administrator mode by right-clicking and selecting "Run as administrator." Enter the following command:
      ```powershell
      wsl --install
      ```
   - Alternatively, you can install other environments such as Cygwin, although detailed instructions for their installation are not explained here.

2. **Open the Linux environment:**
   - Run `Bash` in the command line to enter the Linux environment.

3. **Install necessary tools:**
   - Inside the Linux terminal, run `sudo apt-get install gcc make` to install the GNU Compiler Collection and the make program.

### macOS

- Run the `cc` command. A window should appear asking you to install command line developer tools. You can also run `xcode-select --install` to trigger this window.

### Linux

- In Ubuntu, run `sudo apt-get install gcc make`.
- Other Linux distributions should have `gcc` and `make` packages available.

## Running the Program
You can download the helio compiled executable file and run it in your Unix environement by typing ./helio <fileName> (including a fileName means opening an existing file). You may need to press enter a second time for the program to run.

Alternatively you can use the `make` command (must download the Makefile in the repository to run this command) to compile the file and then run the program by typing ./helio <fileName>.


