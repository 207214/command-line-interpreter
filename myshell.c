# include <stdio.h>
# include <signal.h>
# include <stdlib.h>
# include <unistd.h>
# include <sys/wait.h>
# include <sys/types.h>
# include <ctype.h>
# include <string.h>
# include <fcntl.h>

# define N 20 // characters per word

// Antaranyan Vladimir, autumn 2018

char *home_directory;
char *input_file, *output_file, *tmp_str;
int status, prev_status, ch, next_ch;
pid_t pid;


void
welcome(void)
{
    if (!fork())
    {
        execlp("clear", "clear", (char*)0);
        exit(2);
    }
    wait(NULL);
    printf("[%d]\n\t\t\tWelcome!\n\n>> ", getpid());
    fflush(stdout);
}


void
check_term(void)                      //checking whether there are terminated background processes
{
    while ( (pid = waitpid(-1, &status, WNOHANG)) && (pid != -1) )
    {
        if (WIFSIGNALED(status))
            printf("\n |Process [%d] stopped by the %d signal|\n\n", pid, WTERMSIG(status));
        else if (WIFEXITED(status))
            printf("\n |Process [%d] terminated, exit status: %d|\n\n", pid, WEXITSTATUS(status));
    }
}


void
handler(int s)                    // check after each '^C'
{
    signal(SIGINT, handler); //
    check_term();
    printf("\n>> ");
    fflush(stdout);
}


void
change_rw(int append)
{
    int fd = 0;
    if (input_file)
    {
        if ( (fd = open(input_file, O_RDONLY | O_CREAT, 0644)) == -1 )
        {
            fprintf(stderr, "Error while opening input file\n");
            exit(2);
        }
        dup2(fd, 0);
        close(fd);
    }
    if (output_file)
    {
        if (append)
            fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0644);
        else
            fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1)
        {
            fprintf(stderr, "Error while opening output file\n");
            exit(2);
        }
        dup2(fd, 1);
        close(fd);
    }
}


void
arrow_reaction(int *sym, int *append)     // forming and returning the file name;
{                                              // continue reading input, but inside this function
    int key = 0;
    if (*sym == '>')
        key = 1;
    int k = 0;
    int len = 10;
    tmp_str = (char *)calloc(len, sizeof(len));
    while ( ( ((*sym = getchar()) != EOF)
            && (*sym != ';') && (*sym != '&') && (!isspace(*sym)) ) || (!k))
    {

        if ( (*sym == '<') || (*sym == '>') )
        {
            if ((key) && (*sym == '>'))                        // '>>' - append-case
                *append = 1;
            break;
        }
        if ( (*sym == '"') || (isspace(*sym)) )
            continue;
        if (*sym == '\n')
            break;
        tmp_str[k++] = *sym;
        if (k == len - 1)
        {
            len <<= 1;
            tmp_str = (char *)realloc(tmp_str, len);
        }
    }
    tmp_str[k] = 0;
    if (key)
        output_file = strdup(tmp_str);
    else
        input_file = strdup(tmp_str);
    free(tmp_str), tmp_str = NULL;
}


void
change_directory(char *curr, char *dest, char **prev)
{
    int i = strlen(curr) - 1;//???
    if (!strcmp(dest, "-"))                               // to the previous directory
        i = chdir(*prev);
    else if (!strcmp(dest, ""))                           // no arg  => to the root (/)
        i = chdir("/");
    else if (!strcmp(dest, "~"))
        i = chdir(home_directory);
    else
        i = chdir(dest);

    if (i)
        fprintf(stderr, "Error while changing directory\n");
    else
        strcpy(*prev, curr);
}


void
back_mem(int j, char **b, char *w) //b - buffer, w - word
{

    for (int k = 0; k < j; ++k)
        if (b[k]) free(b[k]);
    if (b)
        free(b), b = NULL;
    if (w)
        free(w), w = NULL;
}


void
back_mem_2(char **curr, char **prev, char **home)
{
    if (*curr)
        free(*curr), *curr = NULL;
    if (*prev)
        free(*prev), *prev = NULL;
    if (*home)
        free(*home), *home = NULL;
}


int
main(void)
{
    int fd1[2];  // in case of '|': reading from fd1[0] (if not first command),
    int fd2[2] = {0,1}; // writing to fd2[1] (if not last command)
    pipe(fd1);

    signal(SIGINT, handler);
    pid = status = 0;

    long max_name = pathconf("/", _PC_PATH_MAX);
    char *previous_directory = (char  *)calloc(max_name, sizeof(char)); // no info at the beginning
    char *current_directory = NULL;
    current_directory = getcwd(current_directory, max_name);  // working directory at the beginning
    home_directory = strdup(current_directory);

    register int i, j, chnum;          // chnum - number of channels (number of '|' before '\n')
    register int space_flag = 1;       // space_flag == 0   if (isspace(previous character)!=0)
    register int quot_mark = 0;        // quot_mark == 1 if the second <"> is expected
    register int spec_flag = 0;        // spec_flag == 1 if '&' has been put
    int append_flag = 0;
    int length = N;
    i = ch = j = chnum = 0;
    next_ch = EOF;

    char **buf = (char **)calloc(0, 0);
    char *word = (char *)calloc(length, sizeof(char));
    if (word == NULL)
    {
        fprintf(stderr, "Error: no memory\n");
        return 1;
    }
    welcome();

    while (ch != EOF)
    {

        if (next_ch != EOF)
            ch = next_ch, next_ch = EOF;
        else
            ch = getchar();

        if (ch == '"')
        {
            quot_mark = (quot_mark + 1) %2;
            continue;
        }

        while ( ( ((ch == '<') || (ch == '>')) && (!quot_mark) ) )
            arrow_reaction(&ch, &append_flag);
        if (ch == EOF)
            break;
        if ( (ch == '&') && (!quot_mark) )                     // normal '&' (not inside "...")
        {
            spec_flag++;
            if (space_flag)
            {
                i = word[i] = 0;
                buf = (char**)realloc(buf, (j+2)*sizeof(char*));
                buf[j++] = strdup(word);
            }
        }


        if ( (ch == '\n') || (ch == '|') || (ch == ';'))
        {

            if  ((!j) && (!i))        // == only 'space'-characters put
            {
                kill(getpid(), SIGINT);
                continue;
            }

            if (space_flag)           // == no 'space-char' before '\n' - last word not formed yet
            {
                i = word[i] = 0;
                buf = (char**)realloc(buf, (j+2)*sizeof(char*));
                buf[j++] = strdup(word);
            }
            buf[j] = NULL;           // execvp() last arg

            if (!strcmp(buf[0],"cd"))
            {
                if (j != 1)                                    // == cd with args
                    change_directory(current_directory, buf[1], &previous_directory);
                else                                           // == cd
                    change_directory(current_directory, "", &previous_directory);
                current_directory = getcwd(current_directory, max_name);
                puts(current_directory);                       //
                back_mem(j, buf, NULL);
                buf = (char**) calloc(0, 0);
                printf("\n>> ");
                j = 0;
                continue;
            }

            if (ch == '|')
            {
                while (isspace(next_ch = getchar()));
                if (next_ch != '|')
                {
                    pipe(fd2);                                     // new channel to write
                    chnum++;
                }
            }
            if ( ( (spec_flag) && ( strcmp(buf[j-1], "&") ) ) ||
                    (quot_mark) || (spec_flag > 1) || ((j == 1)&&(spec_flag)) )    //wrong input
            {
                spec_flag = 0;
                back_mem(j, buf, NULL);
                back_mem_2(&current_directory, &previous_directory, &home_directory);
                quot_mark = j = 0;
                buf = (char**) (char **)calloc(0, 0);
                fprintf(stderr, "Wrong expression\n\n>> ");
                continue;
            }
            if (spec_flag)                            // ignore the last arg which is equal to '&'
                free(buf[j-1]), buf[j-1] = NULL;

                                                                          // not empty and not cd
            pid = fork();
            if (!pid)
            {
                if ( (chnum > 1) || ((chnum == 1) && ((ch == '\n')||(ch == ';'))) )
                {
                    dup2(fd1[0], 0);
                    close(fd1[0]);
                    close(fd1[1]);
                }
                if ( (ch == '|') && (next_ch != '|') )
                {
                    dup2(fd2[1], 1);
                    close(fd2[0]);
                    close(fd2[1]);
                }

                printf("[%d]\n", getpid());
                if ((input_file) || (output_file))
                    change_rw(append_flag);   // dup2
                execvp(buf[0], buf);
                fprintf(stderr, "'%s' not found\n", buf[0]);
                back_mem_2(&current_directory, &previous_directory, &home_directory);
                back_mem(j, buf, word);
                if (spec_flag)
                    printf("\n>> ");
                exit(10);
            }
            if (fd1[0] > 1) close(fd1[0]);
            if (fd1[1] > 1) close(fd1[1]);
            if (ch == '|')
            {
                fd1[0] = fd2[0];     // the next program will read from the channel where we wrote
                fd1[1] = fd2[1];
            }
            else
                fd1[0] = fd1[1] = -1;
            back_mem_2(&input_file, &input_file, &output_file);
            if (!spec_flag)
            {
                waitpid(pid, &status, 0);     // waiting exactly for the 'normal' process
                if ((!status) && (next_ch == '|'))
                {
                    //<(prog1)||\n> - the next line (i.e. prog2) is connected with prog1 through '||'
                    while (isspace(ch = getchar()));
  	            while ( (ch = getchar()) != '\n');
                }
                check_term();        //checking whether there are terminated background processes
            }
            back_mem(j, buf, NULL);
            buf = (char**) calloc(0, 0);
            append_flag = spec_flag = j = 0;
            if ((ch == '\n') || (ch == ';'))
            {
                chnum = 0;
                if (ch == '\n')
                    printf("\n>> ");
            }

        }    // end of '\n'-handling


        else if ( (isspace(ch) )&&(!quot_mark) )
        {
            if (!i) continue;                               // another space
            space_flag = i = word[i] = 0;                   // first space - word not formed yet
            buf = (char **)realloc(buf, (j+2)*sizeof(char*));
            buf[j++] = strdup(word);
        }
        else                             // (normal character) or (space-character inside "...")
        {
            space_flag = 1;
            word[i++] = ch;
            if (i == length - 1)
            {
                length <<= 1;
                word = (char*)realloc(word, length*sizeof(char));
            }
        }
    }

    putchar('\n');
    back_mem_2(&current_directory, &previous_directory, &home_directory);
    back_mem_2(&word, &input_file, &output_file);
    if (fd1[0] >= 0) close(fd1[0]);
    if (fd1[1] >= 0) close(fd1[1]);
    close(fd2[0]);
    close(fd2[1]);
    return 0;
}
