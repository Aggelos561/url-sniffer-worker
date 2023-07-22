# URL Sniffer and Worker

This repository contains an efficient URL Sniffer and Worker implementation designed to find and count URLs in text files. The project is structured using a Makefile, providing easy compilation and execution options. It consists of three main components: `Sniffer`, `Worker`, and `myFunctions`, each playing a crucial role in the system's functioning.

## How It Works

### Sniffer

The Sniffer process is the orchestrator of the entire system. It starts by forking and executing the Listener process, which uses inotifywait to detect file system events. The communication between Sniffer and Listener is achieved using a pipe. When Listener detects a new file creation or modification, it writes the notification data to the pipe, indicating that there's a task to be performed.

Sniffer reads the notifications one by one, ensuring that no commands are missed. Upon receiving a notification, Sniffer checks if there are any available Worker processes to handle the task. If a Worker is available, Sniffer sends the notification to that Worker through a named pipe (FIFO) previously created by Sniffer itself. The Worker then reads the notification, opens the associated URLs file, and performs the URL counting task, writing the results to a corresponding `.out` file.

In the scenario where no Worker is available, Sniffer forks and executes a new Worker process to handle the task. A new named pipe is created for communication between Sniffer and the newly spawned Worker, and the notification is sent to the Worker through this pipe. The Worker executes the URL counting task as described earlier and eventually enters a paused state (SIGSTOP) until Sniffer sends a SIGCONT signal to resume or a SIGTERM signal to terminate.

### Worker

The Worker process is responsible for performing the actual URL counting tasks. It reads the notification data from the named pipe provided by Sniffer and opens the corresponding URLs file. The Worker then creates a new `.out` file with the same name as the URLs file, where the URL counting results will be stored.

Using a carefully designed algorithm, the Worker processes the URLs file efficiently, identifying and counting the URLs that match the HTTP-like format. It writes the results into the `.out` file, which will be later used by the URL Finder script. After completing the task, the Worker raises the SIGSTOP signal, signaling to Sniffer that it's ready for the next task or termination.

### myFunctions

The `myFunctions.cpp` file contains useful utility functions that enhance code clarity, organization, and maintainability throughout the project. These functions are used by both the Sniffer and Worker processes to accomplish various tasks and streamline the codebase.

### URL Finder

The `finder.sh` script is a handy tool for extracting specific top-level domain (TLD) information from the URL counting results. When executed with the appropriate TLD argument(s), the script iterates through all `.out` files located in the `results/` directory. It checks each line for the specified TLD and accumulates the occurrences of that TLD. Finally, it prints the TLD and the total count of occurrences, helping users to quickly analyze the frequency of a particular TLD across multiple files.

## Getting Started

To run this project, follow the steps below:

1. **Compilation**: Use the Makefile to compile the necessary source files.

   ```bash
   make all
   ```

   This command compiles the Sniffer, Worker, and myFunctions source files.

2. **Execution**: To execute the Sniffer, use the following command:

   ```bash
   make run
   ```

   Alternatively, you can specify a specific directory for inotifywait by adding the `-p path` option, e.g., `./sniffer -p /my/directory`.

## Cleaning Up

To clean up named pipes (FIFOs) in the `Named_Pipes/` directory, run:

```bash
make fifo_clean
```

To delete all the result files (`.out` files) in the `results/` directory, run:

```bash
make results_clean
```

## Note

The project is designed with a focus on efficiency, reliability, and ease of use. The use of inotifywait and named pipes allows for real-time handling of URL counting tasks as files are created or modified. The Worker processes are efficiently managed by Sniffer, ensuring optimal utilization of system resources. Additionally, the URL Finder script simplifies the analysis of URL occurrences based on TLDs.

Please note that the URL counting task performed by the Worker process is specifically designed to identify URLs that resemble the HTTP-like format. If your use case requires a more sophisticated URL identification algorithm, you can easily customize the Worker's implementation.