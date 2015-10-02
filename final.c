//Imported libraries

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>
#include<sys/types.h>
#include<string.h>
#include<signal.h>
#include<fcntl.h>
#include<termios.h>
#include<sys/prctl.h>

//The declaration of all the functions for requirements in shell
char* read_command(void);
char** split_command(char*,char*);
void shell_loop(void);
void execute_others(char**,int);
int execute(char**,char*,int);
void echo(char*);
void cd_command(char **);
int getlen(char **);
void print_prompt(char*,char*,char*);
int check_matching(char *,char *);
char *change_name(char*);
char*  redir(char *);
char *getfilename(char*);
int exec_pipes(char **commands);
void restore_streams(void);
int check_piping(char *);
void add_jobs(pid_t,char*);
struct job *find_job(pid_t);
void delete_job(pid_t);
void print_jobs(void);
void foreground_process(pid_t);
pid_t find_pid(int no);
void killall();
void kjob(int pid,int sig);

//Global variables
//Stores the home directory
char home[1024];
int saved_stdout;
int saved_stdin;
pid_t shell_pid;
pid_t shell_pgid;
struct termios shell_tmodes;
int shell_fd;
int interactive;



//The data structure to active and background jobs implemented as a linked list
typedef struct job{
        pid_t pid;  //the process Id 
        char *name;  //The process name
        struct job *next; //The next job in the list
}job; 

job *head,*tail;    //For the linked list



//To add background process to the jobs list
void add_jobs(pid_t pid,char *name)
{
        job *new_job =(job *)malloc(sizeof(job)); //allocating memory for new job
        new_job->pid=pid;
        new_job->name=strdup(name); 
        new_job->next=NULL;
        //fprintf(stderr,"adding a new job to the process list\n");
        if(head==NULL)
        {
                head=new_job;
                tail=new_job;
        }
        else
        {
                //   printf("adding %d %s to list\n",new_job->pid,new_job->name);
                tail->next=new_job;
                tail=tail->next;
        } 
}

//To find the job id for the process 
job *find_job(pid_t pid)
{
        job *top=head;
        while(top!=NULL)
        {
                if(top->pid==pid)
                        return top;
                else
                        top=top->next;
        }
        return NULL;    

}

//To delete job by pid
void delete_job(pid_t pid)
{
        
        job *top=head;
        if(top!=NULL)
        {
                if(top->pid==pid)
                {
                        head=head->next;
                        free(top);
                }
                else{
                        job *to_del = find_job(pid);
                        if(to_del !=NULL)
                        {
                                while(top->next!=NULL)
                                {
                                        if(top->next->pid==pid){
                                                top->next=top->next->next;
                                        }
                                        else
                                            top=top->next;
                                }
                                 free(to_del);
                        }
                }
        }
}
// Function returns the process's Pid based on the number in the linked list
pid_t find_pid(int no)
{
        int i;
        job * top = head;
        for(i=0;i<no-1&&top!=NULL;top=top->next,i++);
        if(top != NULL)
        { 
                pid_t pid = top->pid;
                return pid;
        }
        else
                return 0;
}

//To print the list of active background jobs
void  print_jobs()
{
        job *top=head;
        int i;
        for(i=1;top!=NULL;top=top->next,i++)
        {
                printf("[%d]  %s %d\n",i,top->name,top->pid);
        }
}


int main()
{

        shell_fd=dup(0);
        saved_stdout=dup(1);
        saved_stdin=dup(0); 

        //Set SIGNAL HANDLERS for the shell
        signal (SIGINT, SIG_IGN);
        signal (SIGQUIT, SIG_IGN);
        signal (SIGTSTP, SIG_IGN);
        signal (SIGTTIN, SIG_IGN);
        signal (SIGTTOU, SIG_IGN);
        //Get the PID of the current shell
        shell_pid = getpid();
        //Put the shell in its own process group
        if (setpgid (shell_pid, shell_pid) < 0)
        {
                perror ("Couldn't put the shell in its own process group");
                exit (1);
        }
        //Get the PGID of the shell
        shell_pgid = getpgrp();
        //handle the shell the input output function
        tcsetpgrp(shell_fd,shell_pgid);
        //To make the shell interactive
        interactive=isatty(shell_fd);
        if(interactive)
        {
                while(tcgetpgrp(shell_fd) != (shell_pgid=getpgrp()))
                        kill(- shell_pgid,SIGTTIN);
        }
        //set the home directory
        getcwd(home,sizeof(home));
        //Initialize the pointers in the job list
        head = (job * )malloc(sizeof(job));
        tail = (job * )malloc(sizeof(job));
        head=NULL;
        tail=NULL;
        //the main shell loop
        shell_loop();
        return EXIT_SUCCESS;
}


//Process to handle the cosing of Background process
void bg_close()
{
        //Returns the pid of the exited child process on exit
        int killed=0;
        while((killed=waitpid(-1,NULL,WNOHANG))>0){
                job * front = find_job(killed);
                if (front != NULL)
                {
                    char *name = strdup(front->name);
                    fprintf(stderr,"process %d %s exited ",killed,name);
                    delete_job(killed);
                }
                else
                {
                    
                    fprintf(stderr,"process %d exited ",killed);
                }
        }
}
//The overkill function to kill all background processes
void killall()
{
        while(head!=NULL)
        {
                kill(head->pid,9);
                head = head->next;
        }

}
//The kjob function to send a signal to the function
void kjob(int pid,int sig)
{
        if(kill(pid,sig)<0)
                perror("error sending process\n");
}

//Foregrounding a process
void foreground_process(pid_t pid)
{

        int status;
        pid_t pgid=getpgid(pid);
        fprintf(stderr,"foregrounding process with pid :%d and pgid: %d ",pid,pgid);
        //handling input output filed descriptors to the process
        if(tcsetpgrp(shell_fd,pgid)==-1)
        {
                perror("Error forgrounding process");
        }
        else
        {
                fprintf(stderr,"waiting now\n");
                if(kill(pid,SIGCONT)<0)
                        perror("error continuing process\n");
                job * front = find_job(pid);
                char *name = strdup(front->name);
                delete_job(pid);
                add_jobs(pid,name);//To readd the process if it is backgrounded again
                waitpid(pid,&status,WUNTRACED); //wait for child till it terminates or stops
                if(!WIFSTOPPED(status))
                {   //fprintf(stderr,"process stopped\n");
                    delete_job(pid);}
                //Reset the control back to shell
                tcsetpgrp(shell_fd,shell_pgid);
        }
}
//The main shell function
void shell_loop()
{
        //Set the sginal handler for the child
        signal(SIGCHLD,bg_close);
        int status =1;
        char hostname[1024],cwd[1024],*user;
        //The prompt loop
        while(status)
        {	
                dup2(saved_stdout,1);
                dup2(saved_stdin,0);
                tcsetpgrp(shell_fd,shell_pgid);
                //Functions to get variables for prompts
                gethostname(hostname,sizeof(hostname));
                getcwd(cwd,sizeof(cwd));
                user=getenv("USER");
                //Function to print the prompt
                print_prompt(user,hostname,cwd);
                //read a command
                char* line = read_command();
                //split the command by ';'
                char**command=split_command(line,";");
                int i;
                //execute each command in turn
                for(i=0;command[i]!=NULL&&status;i++)
                {
                    //restore the streams
                        dup2(saved_stdout,1);
                        dup2(saved_stdin,0);
                        tcsetpgrp(shell_fd,shell_pgid);
                        //Check if the command contains piping
                        if(check_piping(command[i]))
                        {
                                char ** pip_cmds=split_command(command[i],"|");
                                status=exec_pipes(pip_cmds);
                                //free the allocated variables
                                free(pip_cmds);
                        }
                        else{//Normal execution

                                char * original_command=strdup(command[i]);
                                command[i]=redir(original_command);
                                original_command=strdup(command[i]);
                                char **args=split_command(command[i]," \n\t");
                                int len=getlen(args);
                                execute(args,original_command,len);
                                free(args);
                                free(original_command);
                        }
                }
                free(command);
                free(line);

        }
}
//Function for executing pipes
/*The pipes implemented involve making a pair of file descriptors 
 * for every part of the pipe and the child passing its output to 
 * its output filed descriptor and then reading form parent's file 
 * descriptor as mentioned in code below the end's of the pipe read 
 * from STD_IN and output to STD_OUT respectively*/

int  exec_pipes(char ** commands)
{
        int n=0;
        int i,j;
        int status=1;
        //int child;
        for(i=0;commands[i]!=NULL;i++)
                n++;
        int *pipes = (int * )malloc(sizeof(int)*2*n);
        for(i=0;i<n;i++)
        {
                pipe(pipes +2*i);
        }
        for(i=0;i<n;i++)
        {
                pid_t pid;
                pid=fork();
                if(pid==0){
                        //Check for pipe's end
                        if(i<n-1)
                        {
                                fprintf(stderr,"duping output stream for %s",commands[i]);
                                //send output to child's output FD
                                if(dup2(pipes[i*2+1],1)<0)
                                {
                                        perror("error in setting filestreams");
                                        return 0;
                                }    
                        }
                        //Check for pipe's start
                        if(i>0)
                        {
                                fprintf(stderr,"duping input stream for %s",commands[i]);
                                //reading from parent's input FD
                                if(dup2(pipes[i*2-2],0)<0)
                                {
                                        perror("error in setting file streamss");
                                        return 0;
                                }
                        }

                        for(j=0;j<2*n;j++){
                                close(pipes[j]);
                        }


                        //Execute The command
                        fprintf(stderr,"for the command %s\n",commands[i]);
                        char * original_command=strdup(commands[i]);
                        commands[i]=redir(original_command);
                        original_command=strdup(commands[i]);
                        char **args=split_command(commands[i]," \n\t");
                        int len=getlen(args);
                        status=execute(args,original_command,len);
                        free(args);
                        free(original_command);
                        exit(1);


                }
                else if(pid<0)
                {
                        perror("error in creating pipes\n");
                        exit(0);
                }
        }
        for(j=0;j<2*n;j++)
        {
                close(pipes[j]);
        }
        for(j=0;j<n;j++)
                wait(NULL);
        free(pipes);
        return status;
}

//To print the prompt
void print_prompt(char *user,char *host,char *cwd)
{
        if(check_matching(home,cwd)==1)
                cwd=change_name(cwd);

        printf("<%s@%s:%s>",user,host,cwd);
}

//Function to check if CWD has an instance of the home folder
int check_matching(char*first,char*second)
{
        int flag=1; 
        if(strlen(first)>strlen(second))
                return 0;
        else 
        {
                int i;
                for(i=0;i<strlen(first);i++)
                        if(first[i]!=second[i])
                        {
                                flag=0;
                                break;
                        }
                return flag;
        }
}

//Change name of the current folder if it contains name of the home folder
char* change_name(char*cwd)
{
        char *dir;
        dir=malloc(sizeof(char)*strlen(cwd)+1);
        dir[0]='~';
        int pos=1;
        int i=strlen(home);
        for(;i<strlen(cwd);i++)
        {
                dir[pos++]=cwd[i];
        }
        dir[pos]='\0';
        return dir;
}

//To get the no of arguments passed to the function
int getlen(char** args)
{
        int i=0;
        for(i=0;args[i]!=NULL;i++);

        return i;
}


//Function to read a command
char * read_command()
{
        char *command=NULL;
        size_t buf=0;
        getline(&command,&buf,stdin);
        return command;	
}


//Function that splits an array based on a delimiter
//used to seperate commands and arguments
char** split_command(char *command,char *delim)
{
        int bufsize = 1024;
        char **commands=malloc(bufsize*sizeof(char*));
        char* line;
        if(!commands)
        {   fprintf(stderr,"memory allocatiion not possible sorry\n");
                exit(0);}
        else
        {//strtok splits the string according to the delimiter
                line=strtok(command,delim);
                int pos=0;
                while(line!=NULL)
                {
                        commands[pos]=line;
                        pos++;
                        line=strtok(NULL,delim);
                        //if the no of arguments is greater than the allocated buffer
                        if (pos>=bufsize){
                                bufsize++;
                                commands=realloc(commands,bufsize*sizeof(commands));
                                if(!commands)
                                {
                                        //if not enough memory is available
                                        fprintf(stderr,"memory allocation failiure\n");
                                }

                        }



                }
                commands[pos]=NULL;
                return commands; 

        }

}

//Function to execut the commands passed 
int execute(char **args,char*command,int len)
{
        if(args[0]==NULL)
                return 1;
        //*********The shell Built ins********************    
        if(strcmp(args[0],"cd")==0)
        {
                cd_command(args);
        }
        else if(strcmp(args[0],"pwd")==0)
        {
                char cwd[1024];
                getcwd(cwd,sizeof(cwd));
                printf("%s\n",cwd);
        }
        else if(strcmp(args[0],"echo")==0)
        {
                echo(command);
        }
        else if(strcmp(args[0],"jobs")==0)
        {
                print_jobs();    
        }
        else if(strcmp(args[0],"fg")==0)
        {
                if(args[1]!=NULL)
                {
                        int job_no;
                        pid_t pid;
                        job_no = atoi(args[1]);
                        pid = find_pid(job_no);
                        if(pid==0)
                                printf("Invalid job no");
                        else
                                foreground_process(pid);
                }
        }
        else if(strcmp(args[0],"overkill")==0)
                killall();
        else if(strcmp(args[0],"kjob")==0)
        {
                int job_no;
                int sig;
                if(args[1]==NULL||!(job_no = atoi(args[1])))
                {
                        fprintf(stderr,"Invalid job no");
                }
                else if(args[2]==NULL||!(sig = atoi(args[2])))
                {
                        fprintf(stderr,"Invalid signal no");
                }
                else 
                {
                        pid_t pid = find_pid(job_no);
                        if(pid==0)
                                fprintf(stderr,"Invalid job no");
                        else
                                kjob(pid,sig);
                }
        }
        else if(strcmp(args[0],"exit")==0)
                exit(1);

        //***********************************************
        else{
                execute_others(args,len);
        }

        return 1;
}


//Function to execute external commands
void execute_others(char** args,int len){
        int bg =0;
        //the process id
        pid_t pid;
        pid=fork();
        //for background if the last charachter or the last argument is '&'
        if(args[len-1][0]=='&')
        {
                bg=1;
                args[len-1]=NULL;
        }

        else if(args[len-1][strlen(args[len-1])-1]=='&')
        {
                bg=1;
                args[len-1][strlen(args[len-1])-1]='\0';
        }
        if(pid==0)
        {
                //Child process
                //for bg process
                pid_t pid = getpid();
                prctl(PR_SET_PDEATHSIG,SIGHUP);
                setpgid(0,0);
                if(!bg)
                {
                        tcsetpgrp(shell_fd,pid);
                }
                signal (SIGINT, SIG_DFL);
                signal (SIGQUIT, SIG_DFL);
                signal (SIGTSTP, SIG_DFL);
                signal (SIGTTIN, SIG_DFL);
                signal (SIGTTOU, SIG_DFL);
                signal (SIGCHLD, SIG_DFL);//Child process
                if(execvp(args[0],args)==-1)
                {
                        perror("error in child process");        
                }
                _exit(0);
        }
        if(pid<0)
        {
                perror("error in forking\n");
                exit(0);
        }
        if(pid>0)
        {
                if(!bg){
                        int status;
                        tcsetpgrp(shell_fd,pid);              //to avoid racing conditions                  
                        add_jobs(pid,args[0]);
                        //child_pid=pid;
                        waitpid(pid,&status,WUNTRACED);             //wait for child till it terminates or stops
                        if(!WIFSTOPPED(status))
                            {   //fprintf(stderr,"process stopped\n");
                                delete_job(pid);}
                        else
                                fprintf(stderr,"\n[%d]+ stopped %s\n",pid,args[0]);
                        
                        tcsetpgrp(shell_fd,shell_pgid);                   //return control of terminal to the shell
                }
                else
                {
                        add_jobs(pid,args[0]);
                }
        }
        fflush(stdout);
}



//The echo function
void echo(char *str)
{
        int i=0,state=0;
        for(i=0;i<strlen(str);i++)
        {
                if (state ==0)
                { 	if(str[i]=='e'&&str[i+1]=='c'&&str[i+2]=='h'&&str[i+3]=='o')            
                        {    
                                state =1;
                                i+=3;
                        }
                }
                else{
                        while(str[i]==' ')
                                i++;
                        int quote=0;
                        int buf=1024;
                        char *printable;
                        int pos=0;
                        printable=(char*)malloc(sizeof(char)*buf);
                        for(;i<strlen(str);i++)
                        {
                                //Checking for quotes
                                if(str[i]=='\"'||str[i]=='\'')
                                        quote ^=1;
                                else if(quote)
                                {
                                        printable[pos]=str[i];
                                        pos++;
                                }
                                else
                                {
                                        if((str[i]==' '&&str[i-1]!=' ')||str[i]!=' ')
                                        {
                                                printable[pos]=str[i];
                                                pos++;
                                        }
                                }
                                //Reallocating if size of buffer exceeds
                                if(pos>=buf)
                                {
                                        buf+=10;
                                        printable=realloc(printable,sizeof(char)*buf);
                                }
                        }

                        printable[pos]='\0';
                        printf("%s",printable);
                }


        }
}


//The cd command
void cd_command(char**args)
{
        if(args[1]==NULL)
        {
                chdir(home);
        }
        else
        {//if directory command contains '~'
                if(args[1][0]=='~')
                {
                        args[1][0]='.';
                        chdir(home);
                }
                if(chdir(args[1])!=0)
                        perror("incorrect path\n");

        }
}
int check_piping(char *s)
{
        if(strchr(s,'|'))
                return 1;
        return 0;
}

//Function to check if the command contains '>>' substring
int check_redir(char *s)
{
    if(strstr(s,">>") != NULL)
            return 1;
    else
            return 0;
}

//I/O redirection
//The string is split by >> > and < considering only the sets 
// < >> | < > 
char*  redir(char *command)
{
        /* Assuming command is of format 
         *  comamnd < file/command > file/command
         */
        char **commands1,** commands2;
        int flag = check_redir (command);
        if(flag)
                commands1=split_command(command , ">>");
        else
                commands1=split_command(command , ">");
        commands2=split_command(commands1[0] , "<"); 
        int fd;
        if(commands1[1]!=NULL){
                char * file1=getfilename(commands1[1]); 
                if(flag)
                {
                        if((fd=open(file1,O_WRONLY|O_CREAT|O_TRUNC))<0)
                        {   
                                perror("file opening error");
                                return commands2[0];
                        }
                }
                else
                {
                        if((fd=open(file1,O_WRONLY|O_CREAT|O_APPEND))<0)
                        {   
                                perror("file opening error");
                                return commands2[0];
                        }
                
                }
                if(dup2(fd,1)<0)
                    {
                            perror("stream copying error");
                    }
                        close(fd);
                
        }   
        if(commands2[1]!=NULL ){
                char *file2=getfilename(commands2[1]);
                if((fd=open(file2,O_RDONLY))<0)
                {   perror("file opening error");}
                else{
                        if( dup2(fd,0)<0)
                        {
                                perror("stream copying error");
                        }
                        close(fd);
                }
        }
        //remaining command
        return commands2[0];
}
//To get the filename for redirection by removing spaces and new lines 
char *getfilename(char *name)
{
        char **names=split_command(name," \t\n");
        return names[0];
}
void restore_streams()
{
        dup2(saved_stdout,1);
        dup2(saved_stdin,0);
}
