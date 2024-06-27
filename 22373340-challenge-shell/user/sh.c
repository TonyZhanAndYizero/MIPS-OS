#include <args.h>
#include <lib.h>

int string_to_int(char str[])
{
	int result = 0;
	int i = 0;

	// Handle potential leading whitespace
	while (str[i] == ' ')
	{
		i++;
	}

	int minus = 0;
	if (str[i] == '-')
	{
		minus = 1;
		i++;
	}

	// Convert each character to the corresponding digit
	while (str[i] >= '0' && str[i] <= '9')
	{
		result = result * 10 + (str[i] - '0');
		i++;
	}
	if(minus)
		result = -result;

	return result;
}

#define MAX_CMD_LENGTH 4096
#define WHITESPACE " \t\r\n"
#define SYMBOLS "<|>&;()"

#define MAXJOBS 128

typedef struct
{
	int job_id;
	int env_id;
	char cmd[4096];
	int status; // 0: Running, 1: Done
} job_t;

job_t jobs[MAXJOBS];
int job_count = 0;
int next_job_id = 1;

void add_job(int env_id, char *cmd)
{
	//printf("add:%s ?", cmd);
	int flag = 0;
	char *s = strchr(cmd, '&');
	if (s)
	{
		//printf("in:%s ?", cmd);
		if (*s == '&' && *(s + 1) != '&')
		{
			flag = 1;
			//printf("flag:%s ?", cmd);
		}
	}
	
	if (job_count < MAXJOBS && flag)
	{
		jobs[job_count].job_id = next_job_id++;
		jobs[job_count].env_id = env_id;
		strcpy(jobs[job_count].cmd, cmd);
		jobs[job_count].status = 0; // Running	
		job_count ++ ;
	}
}

void list_jobs(){
	for (int i = 0; i < job_count; i++)
	{
		if (jobs[i].status == 0)
		{ // Running
			if (syscall_get_env_status(jobs[i].env_id) != ENV_NOT_RUNNABLE && syscall_get_env_status(jobs[i].env_id) != ENV_RUNNABLE)
			{
				jobs[i].status = 1; // Done
			}
		}
		printf("[%d] %-10s 0x%08x %s\n", jobs[i].job_id, jobs[i].status == 0 ? "Running" : "Done", jobs[i].env_id, jobs[i].cmd);
	}
}


void kill_job(char job_id_str[])
{
	int job_id = string_to_int(job_id_str);
	for (int i = 0; i < job_count; i++)
	{
		if (jobs[i].job_id == job_id)
		{
			if(jobs[i].status != 0){
				printf("fg: (0x%08x) not running\n", jobs[i].env_id);
				return;
			}
			//printf("killing: 0x%08x\n", jobs[i].env_id);
			syscall_kill_env(jobs[i].env_id);
			jobs[i].status = 1;
			return;
		}
	}
	printf("fg: job (%d) do not exist\n", job_id);
}

void fg_job(char job_id_str[])
{
	int job_id = string_to_int(job_id_str);
	for (int i = 0; i < job_count; i++)
	{
		if (jobs[i].job_id == job_id)
		{
			if (jobs[i].status != 0)
			{
				printf("fg: (0x%08x) not running\n", jobs[i].env_id);
				return;
			}
			jobs[i].status = 1;
			return;
		}
	}
	printf("fg: job (%d) do not exist\n", job_id);
}
/* Overview:
 *   Parse the next token from the string at s.
 *
 * Post-Condition:
 *   Set '*p1' to the beginning of the token and '*p2' to just past the token.
 *   Return:
 *     - 0 if the end of string is reached.
 *     - '<' for < (stdin redirection).
 *     - '>' for > (stdout redirection).
 *     - '|' for | (pipe).
 *     - 'w' for a word (command, argument, or file name).
 *
 *   The buffer is modified to turn the spaces after words into zero bytes ('\0'), so that the
 *   returned token is a null-terminated string.
 */
int _gettoken(char *s, char **p1, char **p2) {
	*p1 = 0;
	*p2 = 0;
	if (s == 0) {
		return 0;
	}

	while (strchr(WHITESPACE, *s)) {
		*s++ = 0;
	}
	if (*s == 0) {
		return 0;
	}

	if (strchr(SYMBOLS, *s)) {
		if (*s == '>' && *(s + 1) == '>'){
			*p1 = s;
			*s++ = 0;
			*s++ = 0;
			*p2 = s;
			return 'a'; // 'a' for append redirection
		}
		if (*s == '&' && *(s + 1) == '&')
		{
			*p1 = s;
			*s++ = 0;
			*s++ = 0;
			*p2 = s;
			return 'A'; // AND
		}
		if (*s == '|' && *(s + 1) == '|')
		{
			*p1 = s;
			*s++ = 0;
			*s++ = 0;
			*p2 = s;
			return 'O'; // OR
		}
		int t = *s;
		*p1 = s;
		*s++ = 0;
		*p2 = s;
		return t;
	}

	if (*s == '"'){
		*s = 0;
		s++;
		*p1 = s;
		while (*s && (*s != '"')){
			s++;
		}
		*s++ = 0;
		*p2 = s;
		return 'w';
	}


	*p1 = s;
	while (*s && !strchr(WHITESPACE SYMBOLS, *s)) {
		s++;
	}
	*p2 = s;
	return 'w';
}


int gettoken(char *s, char **p1) {
	static int c, nc;
	static char *np1, *np2;

	if (s) {
		nc = _gettoken(s, &np1, &np2);
		return 0;
	}
	c = nc;
	*p1 = np1;
	nc = _gettoken(np2, &np1, &np2);
	return c;
}

#define MAXARGS 128
char globel_cmd[MAX_CMD_LENGTH] = {0};
int father_fktmp = 0;

int condition_flag = 0;

int parsecmd(char **argv, int *rightpipe) {
	int argc = 0;
	while (1) {
		condition_flag = 0;
		char *t;
		int fd, r;
		int fktmp;
		int c = gettoken(0, &t);
		switch (c) {
		case 0:
			return argc;
		case 'w':
			if (argc >= MAXARGS) {
				debugf("too many arguments\n");
				exit();
			}
			argv[argc++] = t;
			break;
		case '<':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: < not followed by word\n");
				exit();
			}
			// Open 't' for reading, dup it onto fd 0, and then close the original fd.
			// If the 'open' function encounters an error,
			// utilize 'debugf' to print relevant messages,
			// and subsequently terminate the process using 'exit'.
			/* Exercise 6.5: Your code here. (1/3) */
			
			if((r = open(t, O_RDONLY)) < 0){
				debugf("open error: cannot open 't'\n");
				exit();
			}

			dup(r, 0);
			close(r);
			break;
		case '>':
			if (gettoken(0, &t) != 'w') {
				debugf("syntax error: > not followed by word\n");
				exit();
			}
			// Open 't' for writing, create it if not exist and trunc it if exist, dup
			// it onto fd 1, and then close the original fd.
			// If the 'open' function encounters an error,
			// utilize 'debugf' to print relevant messages,
			// and subsequently terminate the process using 'exit'.
			/* Exercise 6.5: Your code here. (2/3) */

			if((r = open(t, O_WRONLY | O_CREAT | O_TRUNC)) < 0){
				debugf("open error: cannot open '%s'\n", t);
				exit();
			}

			dup(r, 1);
			close(r);
			break;
		case 'a': // Handle append redirection
			if (gettoken(0, &t) != 'w')
			{
				debugf("syntax error: >> not followed by word\n");
				exit();
			}

			if ((r = open(t, O_WRONLY | O_CREAT | O_APPEND)) < 0)
			{
				debugf("open error: cannot open '%s'\n", t);
				exit();
			}
			
			dup(r, 1);
			close(r);
			break;
		case '|':;
			/*
			 * First, allocate a pipe.
			 * Then fork, set '*rightpipe' to the returned child envid or zero.
			 * The child runs the right side of the pipe:
			 * - dup the read end of the pipe onto 0
			 * - close the read end of the pipe
			 * - close the write end of the pipe
			 * - and 'return parsecmd(argv, rightpipe)' again, to parse the rest of the
			 *   command line.
			 * The parent runs the left side of the pipe:
			 * - dup the write end of the pipe onto 1
			 * - close the write end of the pipe
			 * - close the read end of the pipe
			 * - and 'return argc', to execute the left of the pipeline.
			 */
			int p[2];
			/* Exercise 6.5: Your code here. (3/3) */
	
			if((r = pipe(p)) != 0){
				return r;
			}

			if((r = fork()) < 0){
				return r;
			}
			*rightpipe = r;
			
			if(r == 0){
				dup(p[0], 0);
				close(p[0]);
				close(p[1]);
				return parsecmd(argv, rightpipe);
			}else{
				dup(p[1], 1);
				close(p[1]);
				close(p[0]);
				return argc;
			}
			break;
		case 'A':;
			fktmp = fork();
			if (fktmp == 0) {
				condition_flag = 1;
				return argc;
			} else {
				// wait(fktmp);
				int son;
				int recv = ipc_recv(&son, 0, 0);
				if(recv){//!0 && cmd2(不执行)
					int next_cond = gettoken(0, &t);
					if (next_cond == 0) {
						return 0;
					} else if (next_cond == 'O') {//!0 && cmd2 || cmd3(执行)
						return parsecmd(argv, rightpipe);
					}
				}else{//0 && cmd2
					return parsecmd(argv, rightpipe);
				}
			}
			break;
		case 'O':;
			fktmp = fork();
			if (fktmp == 0) {
				condition_flag = 1;
				return argc;
			} else {
				// wait(fktmp);
				int son;
				int recv = ipc_recv(&son, 0, 0);
				if(recv == 0){//0 || cmd2(不执行)
					while (1)
					{
						int next_cond = gettoken(0, &t);
						if (next_cond == 0)
						{
							return 0;
						}
						else if (next_cond == 'A')
						{ // 0 || cmd2 && cmd3(执行)
							return parsecmd(argv, rightpipe);
						}
					}
				} else {//!0 || cmd2
					return parsecmd(argv, rightpipe);
				}
			}
			break;
		case ';':
			fktmp = fork();
			if (fktmp)
			{
				wait(fktmp);
				return parsecmd(argv, rightpipe);
			}
			else
			{
				return argc;
			}
			break;
		case '&':
			fktmp = fork();
			if (fktmp)
			{
				father_fktmp = fktmp;
				return parsecmd(argv, rightpipe);
			}
			else
			{
				return argc;
			}
			break;
		}
	}

	return argc;
}

char his_cmd[MAX_CMD_LENGTH] = "cat .mosh_history ";

void runcmd(char *s)
{
	char *comment = strchr(s, '#');
	if (comment)
	{
		*comment = '\0';
	}
	
	char *start = strchr(s, '`');
	while (start) 
	{
		int flag = 0;
		for (int ii = 0; ii < start - s; ii++)
		{
			if(s[ii] == '"'){
				flag = 1;
				break;
			}
		}
		if(flag == 1){
			break;
		}
		char *end = strchr(start + 1, '`');
		if (!end)
		{
			debugf("syntax error: unmatched `\n");
			return;
		}

		*end = '\0';
		char subcommand[4096] = {0};
		strcpy(subcommand, start + 1);
		*end = '`';
		
		char output[4096] = {0};
		
		
		int p[2], r;

		if ((r = pipe(p)) != 0){
			exit();
		}

		if ((r = fork()) < 0){
			exit();
		}
		if (r == 0){
			close(p[0]);
			dup(p[1], 0);
			close(p[1]);
			runcmd(subcommand);
			exit();
		}
		else
		{
			close(p[1]);
			int n = 0;
			int offset = 0;

			while ((n = read(p[0], output + offset, 4095 - offset)) > 0)
			{
				offset += n;
			}

			if (offset >= 0)
			{
				output[offset] = '\0'; // 添加字符串终止符
			}
			else
			{
				printf("error in ` when piping.\n");
				exit();
			}
			close(p[0]);
		}

		char newcmd[4096] = {0};
		*start = 0;
		strcpy(newcmd, s);
		*start = '`';
		newcmd[start - s] = '\0';

		strcpy(newcmd + (start - s), output);
		strcpy(newcmd + strlen(newcmd) , end + 1);


		strcpy(s, newcmd);
		start = strchr(s, '`');
	}

	//处理history
	char oldchar = s[7];
	s[7] = 0;
	if (strcmp(s, "history") == 0){
		s[7] = oldchar;
		char oldcmd[4096] = {0};
		strcpy(oldcmd, s);
		s[7] = 0;
		
		strcpy(s, his_cmd);
		strcpy(s + strlen(s), oldcmd + 7);
	} else {
		s[7] = oldchar;
	}

	// 处理jobs
	oldchar = s[4];
	s[4] = 0;
	if (strcmp(s, "jobs") == 0)
	{
		s[4] = oldchar;
		return;
	}
	else
	{
		s[4] = oldchar;
	}

	// 处理kill
	oldchar = s[4];
	s[4] = 0;
	if (strcmp(s, "kill") == 0)
	{
		s[4] = oldchar;
		return;
	}
	else
	{
		s[4] = oldchar;
	}

	// 处理fg
	oldchar = s[2];
	s[2] = 0;
	if (strcmp(s, "fg") == 0)
	{
		s[2] = oldchar;
		return;
	}
	else
	{
		s[2] = oldchar;
	}
	gettoken(s, 0);

	char *argv[MAXARGS];
	int rightpipe = 0;
	int argc = parsecmd(argv, &rightpipe);
	if (argc == 0) {
		return;
	}
	argv[argc] = 0;
	int child = spawn(argv[0], argv);
	// printf("child:%x\n", child);
	if (child >= 0) {
		int son;
		int recv = ipc_recv(&son, 0, 0);
		//printf("recv,ok: %d\n",recv);
		if (condition_flag) {
			int parent_id = syscall_get_parent_envid(0);
			ipc_send(parent_id, recv, 0, 0);
		}
		close_all();
	} else {
		debugf("spawn %s: %d\n", argv[0], child);
	}
	if (rightpipe) {
		wait(rightpipe);
	}
	exit();
}
#define UP 'A'
#define DOWN 'B'
#define HISTFILE ".mosh_history"
#define MAX_HISTORY 20

int his_count = 0;						   // 历史命令总数
int now_index = 0;						   // 当前命令索引
char history[MAX_HISTORY][MAX_CMD_LENGTH]; // 历史命令数组
char tmp_save[MAX_CMD_LENGTH] = {0};

void readline(char *buf, u_int n) {
	int r;
	for (int i = 0; i < n; i++) {
		if ((r = read(0, buf + i, 1)) != 1) {
			if (r < 0) {
				debugf("read error: %d\n", r);
			}
			exit();
		}
		if (buf[i] == '\b' || buf[i] == 0x7f) {
			if (i > 0) {
				i -= 2;
			} else {
				i = -1;
			}
			if (buf[i] != '\b') {
				printf("\b");
			}
		}
		if (buf[i] == '\033')
		{ // 处理方向键
			buf[i] = 0;
			if ((r = read(0, buf + i, 1)) != 1){
				if (r < 0){
					debugf("read error: %d\n", r);
				}
				exit();
			}
			if (buf[i] != '['){
				continue;
			}
			buf[i] = 0;
			i++;
			if ((r = read(0, buf + i, 1)) != 1)
			{
				if (r < 0){
					debugf("read error: %d\n", r);
				}
				exit();
			}
			if ((buf[i] != UP && buf[i] != DOWN)){
				continue;
			}
			int flag = (buf[i] == UP) ? 0 : 1;
			buf[i] = 0;
			i++;
			if (strlen(buf) > 0 && now_index == his_count)
			{
				strcpy(tmp_save, buf);
			}
			 //printf("now:%d his: %d", now_index,his_count);
			if (flag == 0 && now_index > 0)
			{ // 上键
				now_index--;
				// 清除当前行
				printf("\033[B\033[4096D\033[K");
				// 显示历史命令
				strcpy(buf, history[now_index]);
				printf("$ %s", buf);
				i = strlen(buf) - 1;
			}
			else if (flag == 0 && now_index == 0)
			{
				printf("\033[B\033[4096D\033[K");
				printf("$ %s", buf);
				i = strlen(buf) - 1;
			}
			else if (flag == 1 && now_index < his_count)
			{ // 下键
				
				now_index++;
				if (now_index < his_count){
					// 清除当前行
					printf("\033[4096D\033[K");
					// 显示历史命令
					strcpy(buf, history[now_index]);
					//printf("1st now:%d his: %d", now_index, his_count);
					printf("$ %s", buf);
					i = strlen(buf) - 1;
				}else{
					printf("\033[4096D\033[K");
					strcpy(buf, tmp_save);
					//printf("2st now:%d his: %d", now_index, his_count);
					printf("$ %s", buf);
					i = strlen(buf) - 1;
				}
			}
			else if (flag == 1 && now_index == his_count)
			{
				printf("\033[4096D\033[K");
				strcpy(buf, tmp_save);
				//printf("2st now:%d his: %d", now_index, his_count);
				printf("$ %s", buf);
				i = strlen(buf) - 1;
			}
		}
		if (buf[i] == '\r' || buf[i] == '\n') {
			buf[i] = 0;

			if (strlen(buf) > 0)
			{
				// 保存当前命令到历史记录中
				if (his_count < MAX_HISTORY)
				{
					strcpy(history[his_count], buf);
					his_count++;
				}
				else
				{
					// 如果历史记录已满，移除最旧的命令
					for (int j = 1; j < MAX_HISTORY; j++)
					{
						strcpy(history[j - 1], history[j]);
					}
					strcpy(history[MAX_HISTORY - 1], buf);
				}
				now_index = his_count;
				// runcmd("touch .mosh_history");
				// runcmd("history | .mosh_history");
				int fd;
				if ((fd = open("/.mosh_history", O_RDWR | O_CREAT)) < 0) {
					user_panic("open ./mosh_history: %d", fd);
				}

				int n = 0;

				char inputFileHis[MAX_CMD_LENGTH * 10] = {0};
				for (int k = 0; k < his_count; k++){
					strcpy(inputFileHis + strlen(inputFileHis), history[k]);
					strcpy(inputFileHis + strlen(inputFileHis), "\n");
				}
				if ((n = write(fd, inputFileHis, strlen(inputFileHis))) < 0)
				{
					user_panic("write /.mosh_history: %d", n);
				}
			}
			tmp_save[0] = 0;
			return;
		}
	}
	debugf("line too long\n");
	while ((r = read(0, buf, 1)) == 1 && buf[0] != '\r' && buf[0] != '\n') {
		;
	}
	buf[0] = 0;
}

char buf[1024];

void usage(void) {
	printf("usage: sh [-ix] [script-file]\n");
	exit();
}

int main(int argc, char **argv) {
	int r;
	int interactive = iscons(0);
	int echocmds = 0;
	printf("\n:::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	printf("::                                                         ::\n");
	printf("::                     MOS Shell 2024                      ::\n");
	printf("::                                                         ::\n");
	printf(":::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::\n");
	ARGBEGIN {
	case 'i':
		interactive = 1;
		break;
	case 'x':
		echocmds = 1;
		break;
	default:
		usage();
	}
	ARGEND

	if (argc > 1) {
		usage();
	}
	if (argc == 1) {
		close(0);
		if ((r = open(argv[0], O_RDONLY)) < 0) {
			user_panic("open %s: %d", argv[0], r);
		}
		user_assert(r == 0);
	}

	int pipe_fd[2];

	for (;;) {
		if (interactive) {
			printf("\n$ ");
		}
		readline(buf, sizeof buf);

		if (buf[0] == '#') {
			continue;
		}
		if (echocmds) {
			printf("# %s\n", buf);
		}

		if ((r = pipe(pipe_fd)) != 0)
		{
			user_panic("pipe wrong: %d\n",r);
		}
		strcpy(globel_cmd, buf);
		if ((r = fork()) < 0) {
			user_panic("fork: %d", r);
		}
		if (r == 0) {
			close(pipe_fd[0]); // 关闭读端
			runcmd(buf);

			if ((r=write(pipe_fd[1], &father_fktmp, sizeof(father_fktmp))) == -1)
			{
				user_panic("pipe write wrong: %d\n", r);
			}

			close(pipe_fd[1]); // 关闭写端
			exit();
		} else
		{
			close(pipe_fd[1]); // 关闭写端
			wait(r);

			if ((r = read(pipe_fd[0], &father_fktmp, sizeof(father_fktmp))) == -1)
			{
				user_panic("pipe read wrong: %d\n", r);
			}
			close(pipe_fd[0]); // 关闭读端
			//printf("Received from child process: %d, %s\n", father_fktmp, globel_cmd);
			add_job(father_fktmp, globel_cmd);
			
			char *s = buf;
			// printf("%s\n", s);
			// 处理jobs
			char oldchar = s[4];
			s[4] = 0;
			if (strcmp(s, "jobs") == 0)
			{
				s[4] = oldchar;
				list_jobs();
				// printf("nums: %d\n", job_count);
			}
			else
			{
				s[4] = oldchar;
			}

			// 处理kill
			oldchar = s[4];
			s[4] = 0;
			if (strcmp(s, "kill") == 0)
			{
				s[4] = oldchar;
				s = s + 4;
				char job_id_str[MAX_CMD_LENGTH] = {0};
				strcpy(job_id_str, s);

				kill_job(job_id_str);
			}
			else
			{
				s[4] = oldchar;
			}

			// 处理fg
			oldchar = s[2];
			s[2] = 0;
			if (strcmp(s, "fg") == 0)
			{
				s[2] = oldchar;
				s = s + 2;
				char job_id_str[MAX_CMD_LENGTH] = {0};
				strcpy(job_id_str, s);

				fg_job(job_id_str);
			}
			else
			{
				s[2] = oldchar;
			}
		}
	}
	return 0;
}
