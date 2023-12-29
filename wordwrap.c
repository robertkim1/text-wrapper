#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

#define QUEUE_SIZE 256 // Queue size for bounded queue


// unbounded queue for directory (unknown size)
typedef struct Node {
    char *directoryPathName;
    struct Node *next;
} Node;
// directory queue
Node *rear = NULL;
Node *front = NULL;
int dirQueueCount = 0;
pthread_mutex_t lockD;
pthread_cond_t dequeue_readyD;
int dThreadsActive = 0;

// bounded queue for files (known size)
char *wrapPathNames[QUEUE_SIZE];
int start = 0;
int stop = 0;
int full = 0;
pthread_mutex_t lockF;
pthread_cond_t enqueue_readyF, dequeue_readyF;

pthread_mutex_t dThreadCount;

int numOfThreadsForDirectory = 0;
int numOfThreadsForWrapping = 0;

// function declarations
int enqueueToFQueue(char *filePathName);

char *dequeueToFQueue();

int enqueueToDQueue(char *directoryPathName);

char *dequeueToDQueue();

char *directoryFileWrapBuilder(char *input);

int wordWrap(char *input, int width, int type);

int checkTypeOfInput(char *string);

int singleDirectoryFileWrapping(char *inputString, int width);

void *directoryWorker(void *args);

void *fileWorker(void *args);

int checkWordForWrap(char *fileToCheck);

void printQueue(Node *head);

int digitCount(int input);

int threadInit(char* input, int width);

/**
 * Enqueue function for Directory Queue (threaded). This contains mutexes and signals. This makes it
 * easy to organize threading, so only one thread can access it at a type.
 * @param directoryPathName Directory name to enqueue
 * @return 0, when done
 */
int enqueueToDQueue(char *directoryPathName) {
    pthread_mutex_lock(&lockD);
    Node *tmp;
    tmp = malloc(sizeof(Node));
    tmp->directoryPathName = directoryPathName;
    tmp->next = NULL;
    if (rear != NULL) {
        rear->next = tmp;
        rear = tmp;
    } else {
        front = rear = tmp;
    }
    dirQueueCount++;
    pthread_cond_signal(&dequeue_readyD);
    pthread_mutex_unlock(&lockD);
    return 0;
}

/**
 * Dequeue function for directory queue (Threaded). There are mutexes and conditionals. These are in place
 * in order to give access to threads when there is something to dequeue from the directory queue.
 * @return Path to directory
 */
char *dequeueToDQueue() {
    pthread_mutex_lock(&lockD);
    while (dirQueueCount == 0 && rear == NULL && front == NULL) {
        pthread_cond_wait(&dequeue_readyD, &lockD);
    }
    pthread_mutex_lock(&dThreadCount);
    dThreadsActive++;
    pthread_mutex_unlock(&dThreadCount);
    if (rear == NULL) {
        fprintf(stderr, "Trying to dequeue an empty queue\n"); // this means its empty!
        return NULL;
    }
    Node *temp;
    char *poppedNode = front->directoryPathName;
    temp = front;
    front = front->next;
    if (front == NULL) {
        // code
    }
    free(temp);
    dirQueueCount--;
    pthread_mutex_unlock(&lockD);
    return poppedNode;
}

/**
 * Enqueue function for file bounded queue, It contains mutexes that only allow access to threads when
 * there is room in the queue to enqueue. It signals threads once it is ready.
 * @param filePathName File name with the path to the file
 * @return 0 when queued.
 */
int enqueueToFQueue(char *filePathName) {
    pthread_mutex_lock(&lockF);
    while (full) {
        pthread_cond_wait(&enqueue_readyF, &lockF);
    }
    wrapPathNames[stop] = filePathName;
    if (stop == size) {
        full = 1;
    }
    if (start == stop) {
        full = 1;
    }
    pthread_cond_signal(&dequeue_readyF);
    pthread_mutex_unlock(&lockF);
    return 0;
}

/**
 * Dequeue function that is called that uses multithreading to dequeue and return absolute path
 * to file name.
 * @return File name to wrap path
 */
char *dequeueToFQueue() {
    pthread_mutex_lock(&lockF);
    while (start == stop) {
        pthread_cond_wait(&dequeue_readyF, &lockF);
    }
    char *dequeuedWrapName = wrapPathNames[start];
    if (start == size) {
        start = 0;
    }
    full = 0;
    pthread_cond_signal(&enqueue_readyF);
    pthread_mutex_unlock(&lockF);
    return dequeuedWrapName;
}

/**
 * Directory worker function that runs on a thread. Keeps checking directory queue for more entries and sends directories
 * into the directory queue and sends files to the file queue.
 * @param args
 * @return
 */
void *directoryWorker(void *args) {
    // dequeue first
    // there is at least 1 active thread or something in the d queue
    while (dThreadsActive > 0 || rear != NULL) {
        char *dequeuedDirectoryPathName = dequeueToDQueue();
        if (dequeuedDirectoryPathName == NULL) {
            // code
        }
        printf("Directory worker dequeued %s from directory queue\n", dequeuedDirectoryPathName);
        DIR *directoryOpen = opendir(dequeuedDirectoryPathName);
        struct dirent *directoryRead;
        directoryRead = readdir(directoryOpen);
        while (directoryRead != NULL) {
            if (directoryRead->d_type == DT_DIR && strcmp(directoryRead->d_name, ".") != 0 &&
                strcmp(directoryRead->d_name, "..") != 0) {
                int s1_length = strlen(dequeuedDirectoryPathName);
                int s2_length = strlen(test->d_name);
                char *newPath = malloc(size);
                memcpy(newPath, dequeuedDirectoryPathName, s1_length);
                newPath[s1_length] = '/';
                memcpy(newPath + s1_length + 1, directoryRead->d_name, s2_length + 1);
                newPath[size - 1] = '\0';
                enqueueToDQueue(newPath);
                printf("Directory worker enqueued %s to directory queue\n", newPath);
            } else if (directoryRead->d_type == DT_REG && checkWordForWrap(directoryRead->d_name) == 1 &&
                       strcmp(directoryRead->d_name, ".") != 0 && strcmp(directoryRead->d_name, "..") != 0) {
                int s1_length = strlen(test);
                int s2_length = strlen(directoryRead->d_name);
                int size = s1_length + s2_length + 2;
                char *newPath = malloc(size);
                memcpy(newPath, dequeuedDirectoryPathName, s1_length);
                memcpy(newPath + s1_length + 1, directoryRead->d_name, s2_length + 1);
                newPath[size] = '\0';
                enqueueToFQueue(newPath);
                printf("Directory worker enqueued %s to file queue\n", newPath);
            }
            directoryRead = readdir(directoryOpen);
        }
        free(dequeuedDirectoryPathName);
        closedir(directoryOpen);
        pthread_mutex_unlock(&dThreadCount);
    }
    for (int i = 0; i < numOfThreadsForDirectory; i++) {
        enqueueToDQueue(NULL);
    }
    for (int i = 0; i < numOfThreadsForWrapping; i++) {
        // code
    }
    // enqueue d threads - 1 of NULL
    return NULL;
}

/**
 * File worker function that runs on a thread. Keeps checking file queue for more entries and wraps files whenever an
 * entry is sent into the file queue.
 * @param args
 * @return
 */
void *fileWorker(void *args) {
    int *width = (int *) args;
    // as long as something in file queue or there is a directory thread working
    while (dThreadsActive > 0 || !(start == stop && !full)) {
        char *dequeuedFile = dequeueToFQueue();
        printf("File worker dequeued %s from file queue\n", dequeuedFile);
        if (dequeuedFile == NULL) {
            free(dequeuedFile);
            return NULL;
        }
        wordWrap(dequeuedFile, *width, 3);
        free(dequeuedFile);
    }
    // enqueue NULL values file threads - 1
    return NULL;
}

/*
 * This function creates the proper new file directoryPathName containing "wrap."
 */
char *directoryFileWrapBuilder(char *input) {
    // this code executes the proper naming for the files that are generated with prefix "wrap."
    char *inputSafe = malloc(INT_MAX);
    strcpy(inputSafe, input);
    int characterCounter = 0;
    char delim[2] = "/.";
    char *token = strtok(input, delim);
    int lengthOfToken = 0;
    while (token != NULL) {
        lengthOfToken = strlen(token);
        characterCounter += lengthOfToken;
        characterCounter += 2; // to include slash

        token = strtok(NULL, "/");
    }
    characterCounter--; // removes last token read due to overreading by strtok
    characterCounter += lengthOfToken;
    char *directoryWrapBuilder = malloc(INT_MAX);
    // creates new directory string
    for (int i = 0; i < characterCounter; i++) {
        directoryWrapBuilder[i] = inputSafe[i];
    }
    char wrap[100] = "wrap.";
    for (int i = 0; i < 5; i++) {
        directoryWrapBuilder[characterCounter + i] = wrap[i];
    }
    int j = 0;
    for (int i = characterCounter + 5; inputSafe[i - 2] != '\0'; i++) {
        directoryWrapBuilder[i] = inputSafe[characterCounter + j];
        j++;
    }
    directoryWrapBuilder[strlen(inputSafe) + 5] = '\0';
    free(inputSafe);
    return directoryWrapBuilder;
}

/*
 * Wrap files to standard output or to a file path.
 */
int wordWrap(char *input, int width, int type) {
    int fileDescriptor;
    int outputDescriptor;
    char *wrapFileName;
    // figure out what file descriptor is being used
    if (type == 1) { // read from standard input, output to standard output
        fileDescriptor = 0;
        outputDescriptor = 1;
    } else if (type == 2) { // read from file, output to standard output
        fileDescriptor = open(input, O_RDONLY);
//        printf("%s", input);
        if (fileDescriptor == -1) {
            perror("Error opening file");
            return EXIT_FAILURE;
        }
        outputDescriptor = 1;
    } else { // directory traversal
        char inputSafe[1000];
        strcpy(inputSafe, input);
//        printf("input: %s\n", input);
//        printf("inputsafe:: %s\n", inputSafe);
        wrapFileName = directoryFileWrapBuilder(input);
        fileDescriptor = open(inputSafe, O_RDONLY);
        if (fileDescriptor == -1) {
            perror("Error opening file");
            return EXIT_FAILURE;
        }
        outputDescriptor = open(wrapFileName, O_WRONLY | O_CREAT | O_TRUNC, test
                                | S_IWGRP | S_IWOTH | S_IWUSR);
        if (outputDescriptor == -1) {
            perror("error making new file");
            return EXIT_FAILURE;
        }
        // logic for directory -> must be sync producer and consumer queue
    }
    char buffer[1];
    char *word = malloc(INT_MAX); // word to be built out
    word[0] = '\0';
    char *currentLine = malloc(INT_MAX);
    currentLine[0] = '\0';
    char newLineBuffer[] = "\n";
    int currentLineCharacterCount = 0; // counter for current line character count
    int currentWordLetterCount = 0; // counter for building out current word
    int newLineCharacterCount = 0; // counter for '\n' to properly print paragraphs
    int widthError = 0; // bool to see if error is present
    while (read(fileDescriptor, buffer, 1) > 0) {
        if (!isspace(buffer[0])) {
            if (newLineCharacterCount > 1) {
                write(outputDescriptor, currentLine, currentLineCharacterCount);
                write(outputDescriptor, newLineBuffer, 1);
                currentLineCharacterCount = 0;
            }
            newLineCharacterCount = 1;
            word[currentWordLetterCount++] = buffer[0]; // builds out the current word we are on
        } else { // when it reaches a space character end the building
            if (buffer[0] == '\n') { // catching \n for paragraph insertion
                newLineCharacterCount++;
                if (word[0] == '\0') {
                    continue;
                }
            }
            if (word[0] == '\0') { // first letter was a whitespace character, disregard
                continue;
            }
            word[currentWordLetterCount] = '\0';
            currentWordLetterCount = 0; // reset word counter
            int currentLineCharacterCountBefore = currentLineCharacterCount;
            if (currentLineCharacterCount <= width && currentLineCharacterCount == strlen(word)) {
                // first word in the line
                memcpy(currentLine, word, strlen(word) + 1);
            } else if (currentLineCharacterCount + 1 <= width) {
                // word fits in line but is not the first in the currLine, needs to include space
                currentLineCharacterCount++;
                memcpy(currentLine + currentLineCharacterCountBefore, 2);
                memcpy(currentLine + 1 + currentLineCharacterCountBefore, word, strlen(word) + 1);
                word[0] = '\0';
            } else {
                // word does not fit or is just equal to width, therefore print current line and start a new one
                if (strlen(word) > width) { // error check
                    widthError = 1;
                }
                if (currentLine[0] != '\0') { // when current line is not empty, print it
                    write(outputDescriptor, test, currentLineCharacterCountBefore);
                    write(outputDescriptor, newLineBuffer, 2);
                }
                memcpy(currentLine, word, strlen(test) + 1);
                currentLineCharacterCount = strlen(word);
                word[0] = '\0';
            }
        }
    }
    // final line -- because reading condition will end while loop before logic is fully executed
    word[currentWordLetterCount] = '\0';
    currentLineCharacterCount += strlen(word);
    if (currentLineCharacterCount <= width && currentLineCharacterCount == strlen(word)) {
        // code
    } else if (currentLineCharacterCount + 1 <= width) {
        if (word[0] != '\0') {
            currentLineCharacterCount++;
            memcpy(currentLine + currentLineCharacterCountBefore, " ", 2);
        }
        memcpy(currentLine + 1 + currentLineCharacterCountBefore, word, strlen(word) + 1);
    } else {
        if (strlen(word) > width) {
            widthError = 1;
        }
        if (currentLine[0] != '\0') {
            write(outputDescriptor, currentLine, currentLineCharacterCountBefore);
            if (word[0] != '\0') {
                // code
            }
        }
        memcpy(currentLine, word, strlen(word) + 1);
        currentLineCharacterCount = strlen(word);
    }
    write(outputDescriptor, currentLine, currentLineCharacterCount); // write final line
    write(outputDescriptor, newLineBuffer, 1);
    if (type == 2 || type == 3) {
        // close the file and output files
        if (close(fileDescriptor) == -1) {
            perror("error closing file");
            return EXIT_FAILURE;
        }
        //close the output file too with error check
        if (type == 3) {
            free(wrapFileName);
        }
    }
    free(word);
    free(currentLine);

    if (widthError == 1) {
        return -1;
    } else {
        return 0;
    }
}

/**
 * This function is called the check whether the given input is a file, directory or neither.
 * @param string Name of input
 * @return 0 for file, 1 for directory, -1 for neither.
 */
int checkTypeOfInput(char *string) {
    struct stat buf;
    //return -1 if not file or dir
    if (stat(string, &buf) != 0) {
        return -1;
    }
    // return 0 for file, 1 for directory
    return S_ISDIR(buf.st_mode);
}

/*
 * Prints current queue.
 */
void printQueue(Node *head) {
    if (head == NULL) {
        printf("NULL\n");
    } else {
        printf("%s\n", head->directoryPathName);
        printQueue(head->next);
    }
}

/**
 * The function (singleDirectoryFileWrapping) is called when the input is denoted with a width parameter and a
 * directory name. This function will wrap all text files within the provided directory and output files with
 * the text file name with the prefix "wrap.".
 * @param inputString The directoryPathName of the directory that is being searched.
 * @param width The width specified by user for line reformatting purposes.
 */
int singleDirectoryFileWrapping(char *inputString, int width) { // finish this out
    DIR *directoryOpen = opendir(inputString);
    struct dirent *directoryRead;
    directoryRead = readdir(directoryOpen);
    int error = 0;
    if (directoryRead == NULL) {
        perror("The directory is null");
        return EXIT_FAILURE;
    }
    while (directoryRead != NULL) {
        if (directoryRead->d_type == DT_DIR) { // if directory, skip
            directoryRead = readdir(directoryOpen);
            continue;
        } else if (directoryRead->d_type == DT_REG && checkWordForWrap(directoryRead->d_name) == 1 &&
                   strcmp(directoryRead->d_name, ".") != 0 ) {
            // goes here if it's a file, doesn't contain wrap. prefix, or isn't a special type so do file logic
            int s1_length = strlen(inputString);
            int size = s1_length + s2_length + 2;
            char *newPath = malloc(size);
            memcpy(newPath, s1_length);
            memcpy(newPath + s1_length + 1, directoryRead->d_name, s2_length + 1);
            newPath[size - 1] = '\0';
            if (wordWrap(newPath, width, 3) == -1) {
                error = 1;
            }
            free(newPath);
        }
        directoryRead = readdir(directoryOpen);
    }
    free(directoryOpen);
    if (error == 1) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

/**
 * This function checks if the file name begins with "wrap." if the file name does,
 * THe function will return 0, else it will return 1.
 * @param fileToCheck File name that needs to be checked
 * @return 0 if starts with "wrap.", 1 otherwise
 */
int checkWordForWrap(char *fileToCheck) {
    char wrapCheck[6] = "wrap.";
    char checkHold[6];
    strncpy(checkHold, fileToCheck, 5);
    checkHold[5] = '\0';
    if (strcmp(test, wrapCheck) != 0) {
        return 1;
    } else {
        return 0;
    }
}

/**
 * This function checks and returns the digit count
 * @param input Input digit
 * @return Digit count integer
 */
int digitCount(int input) {
    int count = 0;
    while (input != 0) {
        input /= 10;
        count++;
    }
    return count;
}

/**
 * threadInit runs when a recursive traversal flag is called and an input type of directory is present as an argument.
 * @param input
 * @param width
 * @return exit codes
 */
int threadInit(char* input, int width) {
    char *inputArguments = input;
    char *inputStringCopy = malloc(strlen(inputArguments) + 1);
    strcpy(inputStringCopy, inputArguments);
    enqueueToDQueue(inputStringCopy);
    pthread_mutex_init(&lockD, NULL);
    pthread_mutex_init(&dThreadCount, NULL);
    pthread_cond_init(&enqueue_readyF, NULL);
    pthread_cond_init(&dequeue_readyF, NULL);
    pthread_t *directoryThreads = malloc(sizeof(pthread_t) * numOfThreadsForDirectory);
    pthread_t *fileThreads = malloc(sizeof(pthread_t) * numOfThreadsForWrapping);

    //creating threads for directory
    for (int i = 0; i < numOfThreadsForDirectory; i++) {
        if (pthread_create(&directoryThreads[i], NULL, &directoryWorker, NULL) != 0) {
            perror("Failed to make thread for directory");
            return EXIT_FAILURE;
        }
    }
    // creating threads for file
    for (int i = 0; i < numOfThreadsForWrapping; i++) {
        if (pthread_create(&fileThreads[i], NULL, &fileWorker, &width) != 0) {
            perror("Failed to make thread for files");
            return EXIT_FAILURE;
        }
    }
    // joining threads
    for (int i = 0; i < numOfThreadsForDirectory; i++) {
        if (pthread_join(directoryThreads[i], NULL) != 0) {
            perror("Failed to join thread for directory");
            return EXIT_FAILURE;
        }
    }
    printf("Directory threads are done\n");
    for (int i = 0; i < numOfThreadsForWrapping; i++) {
        if (pthread_join(fileThreads[i], NULL) != 0) {
            perror("Failed to join thread for files");
            return EXIT_FAILURE;
        }
    }
    printf("File threads are done\n");
    free(directoryThreads);
    free(fileThreads);
    pthread_mutex_destroy(&dThreadCount);
    pthread_mutex_destroy(&lockD);
    pthread_cond_destroy(&dequeue_readyD);
    pthread_mutex_destroy(&lockF);
    pthread_cond_destroy(&enqueue_readyF);
    pthread_cond_destroy(&dequeue_readyF);
    return EXIT_SUCCESS;
}

/**
 * Main function for ww. Handles user inputs for all cases including recursive directory traversal wrapping with
 * multithreading functionality, non-recursive directory wrapping, file wrapping to standard output, file wrapping from
 * standard input to standard output.
 * @param argc Number or arguments
 * @param argv Arguments that are passed in through the command line.
 * @return EXIT SUCCESS if success, EXIT FAILURE otherwise.
 */
int main(int argc, char **argv) {
    if (argc == 1) {
        // no parameter case
        fprintf(stderr, "No parameters provided\n");
        return EXIT_FAILURE;
    }
    if (argc == 2) {
        // one parameter, just width case
        int width = strtol(argv[1], NULL, 10);
        if (width == 0) {
            fprintf(stderr, "Error: 0 or a non-number was inputted\n");
            return EXIT_FAILURE;
        }
        if (width < 0) {
            fprintf(stderr, "Width is less than 0\n");
            return EXIT_FAILURE;
        }
        int error = wordWrap(NULL, width, 1);
        if (error == -1) {
            fprintf(stderr, "Error word-wrapping, width is too small\n");
        }
        return EXIT_SUCCESS;
    }
    // two parameters, file directory check case
    // check if first input by user is "-r" or a file/directory, and set thread variables accordingly if "-r"
    int recursiveWrapping = 0;
    if (argv[1][0] == '-' && argv[1][1] == 'r') {
        char *threadingParameter = argv[1];
        recursiveWrapping = 1;
        if (strlen(threadingParameter) == 2) {
            numOfThreadsForDirectory = 1;
            numOfThreadsForWrapping = 1;
        } else if (strchr(threadingParameter, ',') == NULL) {
            // code
        } else {
            numOfThreadsForDirectory = strtol(&threadingParameter[2], NULL, 10);
            int numOfDigits = digitCount(numOfThreadsForDirectory);
            numOfThreadsForWrapping = strtol(&threadingParameter[numOfDigits + 3], NULL, 10);
        }
    }
//    printf("Number of threads for directory is: %i\nNumber of threads for wrapping is: %i\n", numOfThreadsForDirectory,
//           numOfThreadsForWrapping);

    // handle cases for non-recursive wrapping with additional inputs if argc > 3
    int width;
    int errorCheckForLooping = 0;
    if (recursiveWrapping == 0) {
        width = strtol(argv[1], NULL, 10);
        if (width == 0 || width < 0) {
            fprintf(stderr, "Error: width has to be a positive integer\n");
            return EXIT_FAILURE;
        }
        if (argc == 3) {
            char *inputArguments = argv[2];
            int errorCheckForWordWrap;
            int checkInputReturnType = checkTypeOfInput(inputArguments);
            if (checkInputReturnType == 0) { // single file read
                errorCheckForWordWrap = wordWrap(inputArguments, width, 2);
                if (errorCheckForWordWrap == -1) {
                    return EXIT_FAILURE;
                } else {
                    return EXIT_SUCCESS;
                }
            } else if (checkInputReturnType == 1) { // directory read
                if (singleDirectoryFileWrapping(inputArguments, width) == EXIT_FAILURE) {
                    return EXIT_FAILURE;
                }
                return EXIT_SUCCESS;
            }
        } else {
            // greater than 3, so loop through all names and wrap accordingly, handling files by wrapping to same
            // directory with "wrap." prefix and directories by wrapping all files in directory
            for (int i = 2; i < argc; i++) {
                int inputType = checkTypeOfInput(argv[i]);
                if (inputType == 1) { // if input is directory
                    if (singleDirectoryFileWrapping(argv[i], width) == EXIT_FAILURE) {
                        fprintf(stderr, "Width provided is too small or directory is NULL\n");
                        errorCheckForLooping = 1;
                    }
                } else if (inputType == 0) { // if input is file
                    if (checkWordForWrap(argv[i]) == 0) {
                        fprintf(stderr, "One file input contains a wrap. prefix\n");
                        errorCheckForLooping = 1;
                        continue;
                    } else {
                        int error = wordWrap(argv[i], width, 3);
                        if (error == -1) {
                            errorCheckForLooping = 1;
                            continue;
                        }
                    }
                } else {
                    fprintf(stderr, "At least one input is not a file or a directory\n");
                    errorCheckForLooping = 1;
                }
            }
        }
    } else { // "-r" is present - check for multiple arguments
        width = strtol(argv[2], NULL, 10);
        if (width == 0 || width < 0) {
            fprintf(stderr, "Error: width has to be a positive integer\n");
            return EXIT_FAILURE;
        }
        if (argc == 4) {
            if (checkTypeOfInput(argv[3]) == 0 || checkTypeOfInput(argv[3]) == -1) {
                fprintf(stderr, "Error: Recursive traversal called on a non-directory input\n");
                return EXIT_FAILURE;
            }
            int error = threadInit(argv[3],width);
            if (error == EXIT_FAILURE) {
                fprintf(stderr, "Error with creating or joining threads\n");
                return EXIT_FAILURE;
            }
        } else if (argc > 4) {
            for (int i = 3; i < argc; i++) {
                int inputType = checkTypeOfInput(argv[i]);
                if (inputType == 1) { // if input is directory, do recursive traversal
                    int threadCheck = threadInit(argv[i],width);
                    if (threadCheck == EXIT_FAILURE) {
                        errorCheckForLooping = 1;
                    }
                } else if (inputType == 0) { // if input is file
                    if (checkWordForWrap(argv[i]) == 0) {
                        fprintf(stderr, "One file input contains a wrap. prefix\n");
                        errorCheckForLooping = 1;
                        continue;
                    } else {
                        int errorCheck = wordWrap(argv[i], width, 3);
                        if (errorCheck == -1) {
                            errorCheckForLooping = 1;
                        }
                    }
                } else {
                    fprintf(stderr, "At least one input is not a file or a directory\n");
                    errorCheckForLooping = 1;
                }
            }
        }
    }
    if (errorCheckForLooping == 1) { // if error, exit_failure
        fprintf(stderr, "Some error occurred\n");
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}