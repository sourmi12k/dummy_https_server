### 1. 本项目的契机

之前观察cmu课程网站的时候发现了15-441/641的作业中有一个实现https的作业，正好我当时也想实现一个https服务器练练手而且这个课程给的参考资料很完善

- 之所以把这个repo设置成public，是因为我实现的内容和15-641中的要求相比差别挺大的，因此我认为公开这个对于15-441/641的教学影响较小，但如果这个已经影响到了15-441/641的教学，请联系我我将把本repo删除 

- 公开的另一个原因是这个版本还有很多想实现的内容找不到合适的实现方式，因此想向大家征求一些改进的意见



this https server differs from 15-441/641 in the following list, if you think this will still affect the teaching  of 15-441/641, please contact me at sourmi12k@163.com, I will delete this repository

|                      dummy_https_server                      |                   cmu 15-441/641 liso                    |
| :----------------------------------------------------------: | :------------------------------------------------------: |
|                     multithread + epoll                      |                  single thread + select                  |
|               parse HTTP requests line by line               |  yacc + lex, parse after receiving all of request head   |
| one process for all dynamic requests, it exits when encountering error or the exit of the server | create a new process every time  receiving a CGI request |

### 2. 项目介绍

#### 2.1 网络编程模型

本项目的网络部分参考了muduo, 《Linux多线程服务端编程-使用muduo网络库》陈硕。使用的多线程IO复用+非阻塞IO的编程模型，可以把项目分成以下几个部分 acceptor, buffer, eventloop, tlsconn, tcpconn,HTTPClient这几个部分， 下面将对以上几部分分别介绍

#### 2.2 acceptor

负责管理监听文件描述符，目前主要有两个文件描述符分别负责管理http和https，每个acceptor有一个handleNewConn的函数，每次完成三次握手建立新的tcp连接之后，acceptor会调用handleNewConn初始化连接的相关资源(这么设计的主要原因是现在需要两种连接普通TCP连接以及使用TLS通道建立的TLS连接)

#### 2.3 buffer

本部分是一个简单的缓冲区的设计，包含readIndex writeIndex

#### 2.4 eventloop

未完待续。。。