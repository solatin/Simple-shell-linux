#include<stdio.h> 
#include<string.h> 
#include<stdlib.h> 
#include<unistd.h> 
#include<sys/types.h> 
#include<sys/wait.h> 
#include<sys/stat.h>
#include<fcntl.h>
#include<time.h>
//#include<readline/readline.h> 
//#include<readline/history.h> 

using namespace std;
#define MAX_LENGTH 80
#define MAX_LIST 5

#define isPipe 1
#define isOutRedirect 2
#define isInRedirect 3
#define isSimpleCommand 4

char* history = nullptr; // bien toan cuc history luu cau lenh truoc nhat

/*Printf "shh>" to the screen*/
void type_prompt()
{
    static int first_time = 1;
    if(first_time) //clear screen for the first time
    {
        const char* CLEAR_SCREEN_ANSI = " \e[1;1H\e[2J";
        write(STDOUT_FILENO, CLEAR_SCREEN_ANSI, 12);
        first_time = 0;
    }
    printf("ssh>");
    fflush(stdout);
}

/*Read input from screen*/
int get_input(char* line)
{
    int count = 0;
    
    //Read one line
    while (true)
    {
        int c = fgetc(stdin);
        line[count++] = (char)c;
        if (c=='\n')
            break;
    }
    if (count != 1) //strlen(line) != 0
    {
        line[count - 1] = 0;        //add NULL
        /* neu history khac rong thi free, sau do copy lenh vao history */
        if (history != nullptr)
            free(history);
        history = (char*)malloc(strlen(line)+1);
        strcpy(history, line);
        return 0;
    }
    
    return 1;
}

/* Execute system command*/
void exec_args(char** parsed) 
{ 
    int should_wait = 1;
    //check if parent process should wait or not
    for (int i = 0; i < MAX_LIST; ++i)
    {
        if (parsed[i] == NULL)
            break;
        if (strcmp(parsed[i], "&") == 0)
        {
            should_wait = 0;
            parsed[i] = NULL; //delete "&" from parsed
        }
        
    }

    // Forking a child
    pid_t pid = fork(); 
    if (pid == -1)
    { 
        printf("\nFailed forking child.."); 
        return; 
    } 
    else if (pid == 0) //child process
    {
        if (execvp(parsed[0], parsed) < 0)
        {
            printf("\nCould not execute command..");
        }
        exit(0);
    }
    else if (pid > 0) //parent process
    {
        if (should_wait)
        {
            wait(NULL); 
        }
        return; 
    } 
}

/* Ham tach cau lenh ra 2 ve, ben trai va phai cua |<> */
#define pipeRedirect "|<>"
char** parse_pipe(char* line)
{
    int bufSize = 2;
    char* pipe;
    char** pipeArr = (char**)malloc(bufSize*sizeof(char*)+1);
    int pos = 0;

    pipe = strtok(line, pipeRedirect);
    pipeArr[0] = pipe;

    pipe = strtok(nullptr, "\n");
    pipeArr[1] = pipe;
    
    pipeArr[2] = nullptr;
    return pipeArr;
}

/*Parse command words*/
#define TOKEN_BUFSIZE 64
#define DELIM " \t\r\n\a"
char** parse_space(char* line)
{
    int bufSize = TOKEN_BUFSIZE;
    int pos = 0;
    char* token;
    char** tokenArr = (char**)malloc( sizeof(char*)*bufSize);

    if (!tokenArr)
    {
        printf("\nAlloction error..");
        exit(EXIT_FAILURE);
    }
    
    token = strtok(line, DELIM);
  
    while(token != NULL)
    {
        tokenArr[pos] = token;
        pos++;

        if (pos >= bufSize)
        {
            bufSize += TOKEN_BUFSIZE;
            tokenArr = (char**) realloc(tokenArr, bufSize*sizeof(char*));
            if (!tokenArr)
            {
                printf("\nAllocation error..");
                exit(EXIT_FAILURE);
            }
        }
        token = strtok(NULL, DELIM);

    }
    tokenArr[pos] = nullptr;
    
    return tokenArr;
}


/* ham chuyen huong output */
void output_redirect(char** command, char** fileName)
{
    // Forking a child
    pid_t pid = fork();

    if (pid == -1)
    {
        printf("\nFailed forking child.."); 
        return;
    }

    if (pid == 0)
    {
        int fd = open(fileName[0], O_CREAT | O_WRONLY , 0666);

        if (fd < 0)
        {
            printf("\nOpen error..");
            return;
        }

        if (dup2(fd, STDOUT_FILENO) < 0)
        {
            printf("\nDup2 error..");
            return;
        }

        close(fd);
        execvp(command[0], command);
        printf("\nExecvp failed.."); 
        exit(0);   
    }

    waitpid(pid, NULL, 0);
   
}


/* ham chuyen huong input */
void input_redirect(char** command, char** fileName)
{
    // Forking a child
    pid_t pid = fork();

    if (pid == -1)
    {
        printf("\nFailed forking child..");
        return;
    }

    if (pid == 0)
    {
        int fd = open(fileName[0], O_RDONLY, 0666);

        if (fd < 0)
        {
            printf("\nOpen failed..");
            return;
        }

        if (dup2(fd, STDIN_FILENO) < 0)
        {
            printf("\nDup2 failed..");
            return;
        }

        close(fd);
        execvp(command[0], command);
        printf("\nExecvp failed..");
        exit(EXIT_FAILURE);
    }

    waitpid(pid, NULL, 0);
 
}

//thuc thi pipe
void execPipe(char** argv1, char** argv2)
{
    int p[2];
    pid_t p1, p2;
    if (pipe(p) < 0)
    {
        printf("\nPipe failed..");
        return;
    }

    p1 = fork();
    if (p1 == -1)
    {
        printf("\nFork failed..");
        exit(0);
    }
    if (p1 == 0)
    {     
        p2 = fork();
        
        if (p2 < 0)
        {
            printf("\nFork failed..");
            exit(0);
        }
        if (p2 == 0) // p2 nhan dau vao tu p1, dong write end p[1] 
        {
            close(p[1]);
            dup2(p[0], STDIN_FILENO);
            close(p[0]);
            if (execvp(argv2[0], argv2))
            {
                printf("\nExecvp failed..");
                exit(EXIT_FAILURE);
            }           
        }
        // tien trinh p1, gui du lieu cho p2, dong read end p[0]
        close(p[0]);
        dup2(p[1], STDOUT_FILENO);
        close(p[1]);
        if (execvp(argv1[0], argv1) < 0)
        {
            printf("\nExecvp failed..");
            exit(EXIT_FAILURE);
        }     
    }
   
    // tien trinh cha
    
    close(p[1]);
    close(p[0]);
    
    waitpid(p1, NULL, 0);
    waitpid(p2, NULL, 0);             
}


/* Ham kiem tra loai cau lenh nhap vao */
int checkType(char* inputLine)
{
    int type = isSimpleCommand;

    for (int i =0; i < strlen(inputLine); i++)
    {
        if (inputLine[i] == '|') 
        {
            type = isPipe;
            break;
        }
        
        else if (inputLine[i] == '>')
        {
            type = isOutRedirect;
            break;
        } 
       
        else if (inputLine[i] == '<') 
        {
            type = isInRedirect;
            break;
        }     
    }
   
    return type;
    
}



int main(void)
{
    char line[MAX_LENGTH];
    int type, status;
    char** argv1;
    char** argv2;
    char** tmp = nullptr;

    while (true)
    {       
        type_prompt();      //display prompt on screen
        if (get_input(line)) //read input from terminal
            continue;
        type = checkType(line);
        // Neu cau lenh la Pipe hay Redirect thi argv1 luu token o ben trai cac dau '|', '<', '>'; argv2 luu token o ben phai cac dau do
        if (type != isSimpleCommand)
        {
            tmp = parse_pipe(line);
            argv1 = parse_space(tmp[0]);
            argv2 = parse_space(tmp[1]);
        }
        else // Neu cau lenh binh thuong thi luu token vao argv1, argv2 bang null
        {
            argv1 = parse_space(line);
            argv2 = nullptr;
        }
        if (strcmp(line, "exit") == 0)  // neu nhap exit
        {
            waitpid(-1, &status, 0);
            break;
        }
        if (strcmp(line, "!!") == 0)    // neu nhap !! de truy cap lich su
        {
            if (history == nullptr) // neu khong co lenh gan day trong lich su
            {
                write(STDOUT_FILENO, "\nNo command in history..", strlen("\nNo command in history.."));
                continue;
            }
            else
            {
                strcpy(line, history);
                write(STDOUT_FILENO, "ssh>", strlen("ssh>"));
                write(STDOUT_FILENO, line, strlen(line));
                write(STDOUT_FILENO, "\n", strlen("\n"));
            }
        }
        if (type == isOutRedirect)
        {
           output_redirect(argv1, argv2);                  
        }
        else if (type == isInRedirect)
        {
           input_redirect(argv1, argv2);
        }
        else if (type == isPipe)
        {
            execPipe(argv1, argv2); 
            usleep(20);
        }
        else
        {
            exec_argv(argv1);        
        }
        
    }
    return 0;
}