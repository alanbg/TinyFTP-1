#include "utils.h"

int send_msg(int connfd, char* message)
{
  int p = 0;
  int len = strlen(message);
  while (p < len) {
    int n = write(connfd, message + p, len - p);
    if (n < 0) {
      sprintf(error_buf, ERROR_PATT, "write", "send_msg");
      perror(error_buf);
      return -1;
    } else {
      p += n;
    }     
  }
  return 0;
}

int send_file(int des_fd, int src_fd, int offset)
{
  struct stat stat_buf;
  fstat(src_fd, &stat_buf);
  lseek(src_fd, offset, SEEK_SET);

  int to_read, fin_read;
  char buf[DATA_BUF_SIZE];

  int remain = stat_buf.st_size - offset;
  printf("Start file transfer...\n");
  printf("%d bytes to send...\n", remain);

  while (remain > 0) {
    to_read = remain < DATA_BUF_SIZE ? remain : DATA_BUF_SIZE;
    fin_read = read(src_fd, buf, to_read);
    if (fin_read < 0) {
      sprintf(error_buf, ERROR_PATT, "read", "send_file");
      perror(error_buf);
      return -1;
    }
    if (write(des_fd, buf, fin_read) == -1) {
      sprintf(error_buf, ERROR_PATT, "write", "send_file");
      perror(error_buf);
      return -1;
    }
    remain -= fin_read;
  }
  printf("Transfer success!\n");
  return 0;
}

int send_file_mt(int des_fd, int src_fd, int offset)
{
  struct stat stat_buf;
  fstat(src_fd, &stat_buf);
  lseek(src_fd, offset, SEEK_SET);

  int to_read;
  int fin_read_1;
  int fin_read_2;
  char buf_1[DATA_BUF_SIZE];
  char buf_2[DATA_BUF_SIZE];

  int remain = stat_buf.st_size - offset;

  printf("Start file transfer...\n");
  printf("%d bytes to send...\n", remain);

  pthread_t pid;

  struct write_para arg;

  to_read = remain < DATA_BUF_SIZE ? remain : DATA_BUF_SIZE;
  fin_read_1 = read(src_fd, buf_1, to_read);
  if (fin_read_1 < 0) {
    sprintf(error_buf, ERROR_PATT, "read", "send_file");
    perror(error_buf);
    return -1;
  }
  remain -= fin_read_1;

  arg.des_fd = des_fd;
  // at least run once to sendout buf_1
  while (remain > 0) {
    // new thread to send buf_1
    arg.buf = buf_1;
    arg.size = fin_read_1;
    pthread_create(&pid, NULL, (void *)write, (void *)&arg);

    // at the same time fill buf_2
    to_read = remain < DATA_BUF_SIZE ? remain : DATA_BUF_SIZE;
    fin_read_2 = read(src_fd, buf_2, to_read);
    if (fin_read_2 < 0) {
      sprintf(error_buf, ERROR_PATT, "read", "send_file");
      perror(error_buf);
      return -1;
    }
    remain -= fin_read_2;

    // sync
    pthread_join(pid, NULL);

    // new thread to send buf_2
    arg.buf = buf_2;
    arg.size = fin_read_2;
    pthread_create(&pid, NULL, (void *)write, (void *)&arg);

    // at the same time fill buf_1
    to_read = remain < DATA_BUF_SIZE ? remain : DATA_BUF_SIZE;
    fin_read_1 = read(src_fd, buf_1, to_read);
    if (fin_read_1 < 0) {
      sprintf(error_buf, ERROR_PATT, "read", "send_file");
      perror(error_buf);
      return -1;
    }
    remain -= fin_read_1;

    // sync
    pthread_join(pid, NULL);
  }
  write(des_fd, buf_1, fin_read_1);
  printf("Transfer success!\n");
  return 0;
}

int recv_file(int des_fd, int src_fd)
{
  int len;
  char buf[DATA_BUF_SIZE];

  while ((len = read(src_fd, buf, DATA_BUF_SIZE)) > 0) {
    if (write(des_fd, buf, len) == -1) {
      sprintf(error_buf, ERROR_PATT, "write", "recv_file");
      perror(error_buf);
      return -1;
    }
  }

  if (len == 0) {
    return 1;
  } else {
    sprintf(error_buf, ERROR_PATT, "read", "recv_file");
    perror(error_buf);
    return -1;
  }
}

int read_msg(int connfd, char* message)
{
  int n = read(connfd, message, 8191);
  if (n < 0) {
    sprintf(error_buf, ERROR_PATT, "read", "read_msg");
    perror(error_buf);
    close(connfd);
    return -1;
  }
  message[n] = '\0';
  return n;
}

void str_lower(char* str)
{
  int p = 0;
  int len = strlen(str);
  for (p = 0; p < len; p++) {
    str[p] = tolower(str[p]);
  }
}

void str_replace(char* str, char src, char des)
{
  char* p = str;
  while (1) {
    p = strchr(p, src);
    if (p) {
      *p = des;
    } else {
      break;
    }
  }
}

void split_command(char* message, char* command, char* content)
{
  char* blank = strchr(message, ' ');

  if (blank != NULL) {
    strncpy(command, message, (int)(blank - message));
    command[(int)(blank - message)] = '\0';
    strcpy(content, blank + 1);
  } else {
    strcpy(command, message);
    content[0] = '\0';
  }
}

int parse_command(char* message, char* content)
{
  char command[16]; // actually all commands are 4 bytes or less
  split_command(message, command, content);
  strip_crlf(command);
  strip_crlf(content);
  str_lower(command);

  int ret = -1;

  if (strcmp(command, USER_COMMAND) == 0) {
    ret = USER_CODE;
  } 
  else if (strcmp(command, PASS_COMMAND) == 0) {
    ret = PASS_CODE;
  }
  else if (strcmp(command, XPWD_COMMAND) == 0) {
    ret = XPWD_CODE;
  }
  else if (strcmp(command, QUIT_COMMAND) == 0) {
    ret = QUIT_CODE;
  }
  else if (strcmp(command, PORT_COMMAND) == 0) {
    ret = PORT_CODE;
  }
  else if (strcmp(command, PASV_COMMAND) == 0) {
    ret = PASV_CODE;
  }
  else if (strcmp(command, RETR_COMMAND) == 0) {
    ret = RETR_CODE;
  }
  else if (strcmp(command, SYST_COMMAND) == 0) {
    ret = SYST_CODE;
  }
  else if (strcmp(command, STOR_COMMAND) == 0) {
    ret = STOR_CODE;
  }
  else if (strcmp(command, TYPE_COMMAND) == 0) {
    ret = TYPE_CODE;
  }
  else if (strcmp(command, ABOR_COMMAND) == 0) {
    ret = ABOR_CODE;
  }
  else if (strcmp(command, LIST_COMMAND) == 0) {
    ret = LIST_CODE;
  }
  else if (strcmp(command, NLST_COMMAND) == 0) {
    ret = NLST_CODE;
  }
  else if (strcmp(command, MKD_COMMAND) == 0) {
    ret = MKD_CODE;
  }
  else if (strcmp(command, CWD_COMMAND) == 0) {
    ret = CWD_CODE;
  }
  else if (strcmp(command, RMD_COMMAND) == 0) {
    ret = RMD_CODE;
  }
  else if (strcmp(command, REST_COMMAND) == 0) {
    ret = REST_CODE;
  }
  else if (strcmp(command, MULT_COMMAND) == 0) {
    ret = MULT_CODE;
  }
  return ret;
}

int connect_by_mode(struct ServerState* state)
{
  int connfd = state->command_fd;
  if (state->trans_mode == PORT_CODE) {
    if (connect(
          state->data_fd,
          (struct sockaddr*)&(state->target_addr),
          sizeof(state->target_addr)
        ) < 0) {
      sprintf(error_buf, ERROR_PATT, "connect", "command_stor");
      perror(error_buf);
      send_msg(connfd, RES_FAILED_CONN);
      return -1;
    }
  } else if (state->trans_mode == PASV_CODE){
    if ((state->data_fd = accept(state->listen_fd, NULL, NULL)) == -1) {
      sprintf(error_buf, ERROR_PATT, "accept", "command_stor");
      perror(error_buf);
      send_msg(connfd, RES_FAILED_LSTN);
      return -1;
    }
    close(state->listen_fd);
  } else {
    send_msg(connfd, RES_WANTCONN);
    return -1;
  }
  return 0;
}

int close_connections(struct ServerState* state)
{
  close(state->data_fd);
  state->data_fd = -1;
  state->trans_mode = -1;
  return 0;
}

int parse_addr(char* content, char* ip)
{
  str_replace(content, ',', '.');

  int i = 0;
  char* dot = content;
  for (i = 0; i < 4; ++i) {
    dot = strchr(++dot, '.');
  }

  // retrieve ip address
  strncpy(ip, content, (int)(dot - content));
  strcat(ip, "\0");

  // retrieve port 1
  ++dot;
  char* dot2 = strchr(dot, '.');
  char buf[32];
  strncpy(buf, dot, (int)(dot2 - dot));
  strcat(buf, "\0");
  int p1 = atoi(buf);

  //retrieve port 2
  int p2 = atoi(strcpy(buf, dot2 + 1));

  return (p1 * 256 + p2);
  // return 0;
}

int parse_argv(int argc, char** argv, char* hip, char* hport, char* root)
{
  hip[0] = '\0';
  hport[0] = '\0';
  root[0] = '\0';
  struct option opts[] = {
    {"ip-address", required_argument, NULL, 'a'},
    {"port",       optional_argument, NULL, 'p'},
    {"root",       optional_argument, NULL, 'r'}
  };
  int opt;
  while ((opt = getopt_long(argc, argv, "a:p:r:", opts, NULL)) != -1) {
    switch (opt) {
      case 'a':
        strcpy(hip, optarg);
        break;

      case 'p':
        strcpy(hport, optarg);
        break;

      case 'r':
        strcpy(root, optarg);
        break;

      case '?':
       printf("Unknown option: %c\n", (char)optopt);
       break;

      default:
        sprintf(error_buf, ERROR_PATT, "getopt_long", "parse_argv");
        perror(error_buf);
        break;
    }
  }

  if (strlen(hport) == 0) {
    strcpy(hport, "21");
  }

  if (strlen(root) == 0) {
    strcpy(root, "/tmp");
  }

  return 0;
}

void strip_crlf(char* str)
{
  int len = strlen(str);
  if (len < 1) {
    return;
  }
  if (str[len - 1] == '\n' || str[len - 1] == '\r') {
    str[len - 1] = '\0';
    if (len < 2) {
      return;
    }
    if (str[len - 2] == '\n' || str[len - 2] == '\r') {
      str[len - 2] = '\0';
    }
  }
}

int command_user(struct ServerState* state, char* uname)
{
  int ret = 0;
  int connfd = state->command_fd;
  //if (strcmp(uname, USER_NAME) == 0) {
  if (1) {
    send_msg(connfd, RES_ACCEPT_USER);
    ret = 1;
  } else {
    send_msg(connfd, RES_REJECT_USER);
  }

  return ret;
}

int command_pass(struct ServerState* state, char* pwd)
{
  int connfd = state->command_fd;

  //if (strcmp(pwd, PASSWORD) == 0) {
  if (1) {
    send_msg(connfd, RES_ACCEPT_PASS);
    state->logged = 1;
  } else {
    send_msg(connfd, RES_REJECT_PASS);
    state->logged = 0;
  }

  return state->logged;
}

int command_unknown(struct ServerState* state)
{
  int connfd = state->command_fd;
  send_msg(connfd, RES_UNKNOWN);
  return 0;
}

int command_port(struct ServerState* state, char* content)
{
  int connfd = state->command_fd;
  struct sockaddr_in* addr = &(state->target_addr);

  if ((state->data_fd = socket(AF_INET, SOCK_STREAM,  IPPROTO_TCP)) == -1) {
    sprintf(error_buf, ERROR_PATT, "scoket", "aommand_port");
    perror(error_buf);
    send_msg(connfd, RES_REJECT_PORT);
    return -1;
  }

  // check
  if (!strlen(content)) {
    send_msg(connfd, RES_ACCEPT_PORT);
    return 1;
  }

  char ip[64];
  int port = parse_addr(content, ip);
  memset(addr, 0, sizeof(*addr));
  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);

  // translate the decimal IP address to binary
  if (inet_pton(AF_INET, ip, &(addr->sin_addr)) <= 0) {
    sprintf(error_buf, ERROR_PATT, "inet_pton", "command_port");
    perror(error_buf);
    send_msg(connfd, RES_REJECT_PORT);
    return -1;
  }

  send_msg(connfd, RES_ACCEPT_PORT);
  state->trans_mode = PORT_CODE;
  return 1;
}

int command_pasv(struct ServerState* state)
{
  int connfd = state->command_fd;
  char* hip = state->hip;
  struct sockaddr_in* addr = &(state->target_addr);

  if ((state->listen_fd = socket(AF_INET, SOCK_STREAM,  IPPROTO_TCP)) == -1) {
    sprintf(error_buf, ERROR_PATT, "scoket", "command_pasv");
    perror(error_buf);
    send_msg(connfd, RES_REJECT_PASV);
    return -1;
  }

  int p1, p2;
  int port = get_random_port(&p1, &p2);

  memset(addr, 0, sizeof(*addr));
  addr->sin_family = AF_INET;
  addr->sin_port = htons(port);
  addr->sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(state->listen_fd, (struct sockaddr*)addr, sizeof(*addr)) == -1) {
    sprintf(error_buf, ERROR_PATT, "bind", "command_pasv");
    perror(error_buf);
    send_msg(connfd, RES_REJECT_PASV);
    return -1;
  }

  if (listen(state->listen_fd, 10) == -1) {
    sprintf(error_buf, ERROR_PATT, "listen", "command_pasv");
   perror(error_buf);
    send_msg(connfd, RES_REJECT_PASV);
    return -1;
  }

  char hip_cp[32] = "";
  strcpy(hip_cp, hip);
  str_replace(hip_cp, '.', ',');
  char buffer[32] = "";
  sprintf(buffer, RES_ACCEPT_PASV, hip_cp, p1, p2);
  send_msg(connfd, buffer);
  state->trans_mode = PASV_CODE;

  return 1;
}

int command_quit(struct ServerState* state)
{
  int connfd = state->command_fd;
  send_msg(connfd, RES_CLOSE);
  close(connfd);
  return 0;
}

int command_retr(struct ServerState* state, char* path)
{
  int connfd = state->command_fd;

  if (access(path, 4) != 0) { // 4: readable
    send_msg(connfd, RES_TRANS_NOFILE);
    return -1;
  }

  int src_fd;
  if ((src_fd = open(path, O_RDONLY)) == 0) {
    send_msg(connfd, RES_TRANS_NREAD);
    return -1;
  }

  if (connect_by_mode(state) != 0) {
    return -1;
  }

  send_msg(connfd, RES_TRANS_START);
  if (send_file(state->data_fd, src_fd, state->offset) == 0) {
    send_msg(connfd, RES_TRANS_SUCCESS);
  } else {
    send_msg(connfd, RES_TRANS_FAIL);
  }

  close_connections(state);
  state->offset = 0;
  return 0;
}

int command_stor(struct ServerState* state, char* path)
{
  int connfd = state->command_fd;

  int des_fd;
  if ((des_fd = open(path, O_WRONLY | O_CREAT)) == 0) {
    send_msg(connfd, RES_TRANS_NCREATE);
    return -1;
  }

  if (connect_by_mode(state) != 0) {
    return -1;
  }

  send_msg(connfd, RES_TRANS_START);
  if (recv_file(des_fd, state->data_fd) == 0) {
    send_msg(connfd, RES_TRANS_SUCCESS);
  } else {
    send_msg(connfd, RES_TRANS_FAIL);
  }

  close_connections(state);
  return 0;
}

int command_type(struct ServerState* state, char* content)
{
  str_lower(content);
  if (content[0] == 'i' || content[0] == 'l') {
    state->binary_flag = 1;
    send_msg(state->command_fd, RES_ACCEPT_TYPE);
  } else if (content[0] == 'a') {
    state->binary_flag = 0;
    send_msg(state->command_fd, RES_ACCEPT_TYPE);
  } else {
    send_msg(state->command_fd, RES_ERROR_ARGV);
  }
  return 0;
}

int command_list(struct ServerState* state, char* path, int is_long)
{
  if (strlen(path) == 0) {
    path = "./";
  }
  
  int connfd = state->command_fd;

  if (access(path, 0) != 0) { // 0: existence
    send_msg(connfd, RES_TRANS_NOFILE);
    return -1;
  }

  char cmd[128];
  if (is_long) {
    sprintf(cmd, "ls -l %s", path);
  } else {
    sprintf(cmd, "ls %s", path);
  }
  
  FILE* fp = popen(cmd, "r");

  if (!fp) {
    sprintf(error_buf, ERROR_PATT, "popen", "command_list");
    perror(error_buf);
    send_msg(connfd, RES_TRANS_NREAD);
  }

  if (connect_by_mode(state) != 0) {
    return -1;
  }

  send_msg(connfd, RES_TRANS_START);

  char buf[128];
  // if (is_long) {
  //   fgets(buf, sizeof(buf), fp); // remove the first line
  // }
  while (fgets(buf, sizeof(buf), fp)) {
    strip_crlf(buf);
    strcat(buf, "\r\n");
    printf("%s", buf);
    send_msg(state->data_fd, buf);
  }

  printf("Command list finish transfer.\n");
  send_msg(connfd, RES_TRANS_SUCCESS);
  pclose(fp);
  close_connections(state);
  return 0;
}

int command_mkd(struct ServerState* state, char* path)
{
  int flag = mkdir(path, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
  if (flag == 0) {
    send_msg(state->command_fd, RES_ACCEPT_MKD);
  } else {
    send_msg(state->command_fd, RES_REJECT_MKD);
  }
  return flag;
}

int command_cwd(struct ServerState* state, char* path)
{
  int connfd = state->command_fd;
  if (chdir(path) == -1) {
    send_msg(connfd, RES_REJECT_CWD); 
  } else {
    send_msg(connfd, RES_ACCEPT_CWD);
  }
  return 0;
}

int command_rmd(struct ServerState* state, char* path)
{
  int connfd = state->command_fd;
  char cmd[32] = "rm -rf ";
  strcat(cmd, path);
  if (system(cmd) == 0) {
    send_msg(connfd, RES_ACCEPT_RMD);
  } else {
    send_msg(connfd, RES_REJECT_RMD);
  }
  return 0;
}

int command_rest(struct ServerState* state, char* content)
{
  state->offset = atoi(content);
  if (state->offset > 0) {
    send_msg(state->command_fd, RES_ACCEPT_REST);
  } else {
    state->offset = 0;
    send_msg(state->command_fd, RES_REJECT_REST);
  }
  return 0;
}

int command_mult(struct ServerState* state)
{
  if (state->thread == 1) {
    state->thread = 2;
    send_msg(state->command_fd, RES_MULTIT_ON);
  } else {
    state->thread = 1;
    send_msg(state->command_fd, RES_MULTIT_OFF);
  }
  return 0;
}

int get_random_port(int* p1, int* p2)
{
  int port = rand() % (65535 - 20000) + 20000;
  *p1 = port / 256;
  *p2 = port % 256;
  return port;
}



