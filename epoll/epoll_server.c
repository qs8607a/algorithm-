//服务端源码:epoll_server.cpp

#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAXLINE 1024
#define OPEN_MAX 100
#define LISTENQ 20
#define SERV_PORT 5555
#define INFTIM 1000

//线程池任务队列结构体
struct task{
    int fd;            //需要读写的文件描述符
    struct task *next; //下一个任务
};

//用于保存向客户端发送一次消息所需的相关数据
struct user_data{
    int fd;
    unsigned int n_size;
    char line[MAXLINE];
};

//线程的任务函数
void * readtask(void *args);
void * writetask(void *args);

//声明epoll_event结构体的变量,ev用于注册事件,数组用于回传要处理的事件
struct epoll_event ev,events[20];
int epfd;
pthread_mutex_t mutex;
pthread_cond_t cond1;
struct task *readhead=NULL,*readtail=NULL,*writehead=NULL;

void setnonblocking(int sock)
{
    int opts;
    opts=fcntl(sock, F_GETFL);
    if(opts<0)
    {
        perror("fcntl(sock,GETFL)");
        exit(1);
    }
    opts = opts | O_NONBLOCK;
    if(fcntl(sock, F_SETFL, opts)<0)
    {
        perror("fcntl(sock,SETFL,opts)");
        exit(1);
    }
}

int main()
{
    int i, maxi, listenfd, connfd, sockfd,nfds;
    pthread_t tid1,tid2;
    struct task *new_task = NULL;
    struct user_data *rdata = NULL;
    socklen_t clilen;
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond1, NULL);
    //初始化用于读线程池的线程，开启两个线程来完成任务，两个线程会互斥地访问任务链表
    pthread_create(&tid1, NULL, readtask, NULL);
    pthread_create(&tid2, NULL, readtask, NULL);

    //生成用于处理accept的epoll专用的文件描述符  
    epfd = epoll_create(256);

    struct sockaddr_in clientaddr;
    struct sockaddr_in serveraddr;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    //把socket设置为非阻塞方式
    setnonblocking(listenfd);
    //设置与要处理的事件相关的文件描述符
    ev.data.fd = listenfd;

    //设置要处理的事件类型，当描述符可读时出发，出发方式为ET模式
    ev.events = EPOLLIN | EPOLLET;

    //注册epoll事件
    epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);
    bzero(&serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    const char *local_addr = "127.0.0.1";
    inet_aton(local_addr, &(serveraddr.sin_addr));

//htons(SERV_PORT);
    serveraddr.sin_port=htons(SERV_PORT);
    bind(listenfd,(sockaddr *)&serveraddr, sizeof(serveraddr));

    //开始监听
    listen(listenfd, LISTENQ);
    maxi = 0;
    while(1) {
        //等待epoll事件的发生
        nfds=epoll_wait(epfd, events, 20, 500);
        //处理所发生的所有事件    
   for(i=0; i < nfds; ++i)
        {
    if(events[i].data.fd==listenfd)
    {
     connfd = accept(listenfd,(sockaddr *)&clientaddr, &clilen);
     if(connfd<0)
     {
      perror("connfd<0");
      exit(1);
     }
     setnonblocking(connfd);
     const char *str = inet_ntoa(clientaddr.sin_addr);
     std::cout<<"connec_ from >> " << str << std::endl;
     //设置用于读操作的文件描述符
     ev.data.fd=connfd;
     //设置用于注测的读操作事件
     ev.events=EPOLLIN | EPOLLET;
     //注册ev
     epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &ev);
    }
    else if(events[i].events & EPOLLIN)
    {
     printf("reading!\n");               
     if ( (sockfd = events[i].data.fd) < 0) continue;
     new_task = new task();
     new_task->fd =sockfd;
     new_task->next = NULL;
     //添加新的读任务
     pthread_mutex_lock(&mutex);
     if(readhead == NULL)
     {
      readhead = new_task;
      readtail = new_task;
     }  
     else
     {  
      readtail->next = new_task;
      readtail = new_task;
     }  
     //唤醒所有等待cond1条件的线程
     pthread_cond_broadcast(&cond1);
     pthread_mutex_unlock(&mutex);
    }
    else if(events[i].events & EPOLLOUT)
    {  
     rdata=(struct user_data *)events[i].data.ptr;
     sockfd = rdata->fd;
     write(sockfd, rdata->line, rdata->n_size);
     delete rdata;
     //设置用于读操作的文件描述符
     ev.data.fd=sockfd;
     //设置用于注测的读操作事件
     ev.events=EPOLLIN | EPOLLET;
     //修改sockfd上要处理的事件为EPOLIN
     epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
    }
         }
     }
}

void * readtask(void *args)
{
    int fd=-1;
    unsigned int n;
    //用于把读出来的数据传递出去
    struct user_data *data = NULL;
    while(1){
        //互斥访问任务队列
        pthread_mutex_lock(&mutex);
        //等待到任务队列不为空
        while(readhead == NULL)
             pthread_cond_wait(&cond1, &mutex); //线程阻塞，释放互斥锁，当等待的条件等到满足时，它会再次获得互斥锁
        fd = readhead->fd;
        //从任务队列取出一个读任务
        struct task *tmp = readhead;
        readhead = readhead->next;
        delete tmp;
        pthread_mutex_unlock(&mutex);
        data = new user_data();
        data->fd=fd;
        if ( (n = read(fd, data->line, MAXLINE)) < 0)
   		{
            if (errno == ECONNRESET)
                close(fd);
            else
                std::cout<<"readline error"<< std::endl;

            if(data != NULL) delete data;
        }
   else if (n == 0)
   {
    //客户端关闭了，其对应的连接套接字可能也被标记为EPOLLIN，然后服务器去读这个套接字
    //结果发现读出来的内容为0，就知道客户端关闭了。
    close(fd);
    printf("Client close connect!\n");
    if(data != NULL) delete data;
   }
   else
   {
    std::cout << "read from client: " << data->line << std::endl;
    data->n_size = n;
    //设置需要传递出去的数据
    ev.data.ptr = data;
    //设置用于注测的写操作事件
    ev.events = EPOLLOUT | EPOLLET;
    //修改sockfd上要处理的事件为EPOLLOUT
    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
        }
    }
}

//客户端源码：epoll_client.cpp

#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

int main(int argc,char *argv[])
{
    int connect_fd;
    int ret;
    char snd_buf[1024];
    int i;
    int port;
    int len;
    static struct sockaddr_in srv_addr;
    if(argc!=3){
        printf("Usage: %s server_ip_address port\n",argv[0]);
        return 1;
    }   
    port=atoi(argv[2]);
    connect_fd=socket(PF_INET,SOCK_STREAM,0);
    if(connect_fd<0){
        perror("cannot create communication socket");
        return 1;
    }   
    memset(&srv_addr,0,sizeof(srv_addr));
    srv_addr.sin_family=AF_INET;
    srv_addr.sin_addr.s_addr=inet_addr(argv[1]);
    srv_addr.sin_port=htons(port);

    ret=connect(connect_fd,(struct sockaddr*)&srv_addr,sizeof(srv_addr));
    if(ret==-1){
        perror("cannot connect to the server");
        close(connect_fd);
        return 1;
    }
    memset(snd_buf,0,1024);
    while(1){
        write(STDOUT_FILENO,"input message:",14);
        bzero(snd_buf, 1024);
        len=read(STDIN_FILENO,snd_buf,1024);
        if(snd_buf[0]=='@')
            break;
        if(len>0)
            write(connect_fd,snd_buf,len);
        len=read(connect_fd,snd_buf,len);
        if(len>0)
            printf("Message from server: %s\n",snd_buf);
    }
    close(connect_fd);
    return 0;
}//end
