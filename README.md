## 简介

简易的联机版五子棋，C语言编写。

分为三个程序，白棋客户端，黑棋客户端和服务端。

客户端界面：

<img src="https://github.com/user-attachments/assets/2604122b-e17f-4649-aa33-37f8510bd0de" width="50%" />

<img src="https://github.com/user-attachments/assets/61517a50-b910-4aef-955a-bf06ce2667cb" width="50%" />

> 黑棋是 *  白棋是 o

服务端界面：

<img src="https://github.com/user-attachments/assets/dece8481-3af4-48b8-9281-5d9f380d9ff1" width="50%" />

## 使用

```bash
git clone https://github.com/flying-pig-z/Online-Gomoku

cd /Online-Gomoku

gcc -o server server.c
gcc -o black-client black-client.c 
gcc -o white-client white-client.c

./server
./black-client
./white-client

```

> 当然也可以直接运行program目录中编译好的程序
