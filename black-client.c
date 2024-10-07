#include <arpa/inet.h>  // 包含 sockaddr_in 的定义
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>  // 包含 socket 函数的定义
#include <termios.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <wait.h>  // 添加这一行以包含wait函数
#include <fcntl.h>
#include <time.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8848
#define BUFFER_SIZE 1024
#define BOARD_ROWS 50
#define BOARD_COLS 30
#define FILE_NAME "black.txt"
#define PLAYER 0 // 0 为黑色一方，1 为白色一方

// 定义一个结构体来包含共享变量
struct SharedData {
	int arr[BOARD_ROWS][BOARD_COLS];            // 生成棋盘的数组
	int x;                      // 光标坐标x轴
	int y;                      // 光标坐标y轴
	int judgeWhoPlayChess;      // 轮到谁下棋，0为正方下，1为反方下
	int judge;              // 0为还未有结果，1为已经胜利，2为还未胜利
};

// 全局指针用于访问共享内存
struct SharedData *shared_data;


// 创建和初始化共享内存
void createAndInitializeSharedMemory() {
	int shm_id = shmget(IPC_PRIVATE, sizeof(struct SharedData), IPC_CREAT | 0666);
	if (shm_id < 0) {
		perror("shmget failed");
		exit(1);
	}

	shared_data = (struct SharedData *)shmat(shm_id, NULL, 0);
	if (shared_data == (struct SharedData *)(-1)) {
		perror("shmat failed");
		exit(1);
	}

	// 初始化
	for (int i = 0; i < BOARD_ROWS; i++)
		for (int j = 0; j < BOARD_COLS; j++)
			shared_data->arr[i][j] = '.'; //初始化棋盘
	shared_data->x = 0; // 初始化x坐标
	shared_data->y = 0; // 初始化y坐标
	shared_data->judgeWhoPlayChess = 0; // 初始化轮到谁下棋
	shared_data->judge=0; // 初始化棋盘结果
}




/*
    基础函数
*/

void clearScreen() {
	printf("\033[H\033[J");  // 清屏并将光标移动到左上角
}

int getch() {
	int buf = 0;
	struct termios old = {0};

	if (tcgetattr(0, &old) < 0)
		perror("tcgetattr()");

	old.c_lflag &= ~ICANON;  // 禁用规范模式
	old.c_lflag &= ~ECHO;    // 禁用回显
	if (tcsetattr(0, TCSANOW, &old) < 0)
		perror("tcsetattr()");

	if (read(0, &buf, 1) < 0)
		perror("read()");

	old.c_lflag |= ICANON;  // 重新启用规范模式
	old.c_lflag |= ECHO;    // 重新启用回显
	tcsetattr(0, TCSANOW, &old);

	return buf;  // 返回整型值
}


/*
    业务相关小模块
*/

// 根据输入移动光标
void moveCursor(int input) {
	// 保存改变前的坐标
	int pastX = shared_data->x;
	int pastY = shared_data->y;

	// 改变坐标
	if (input == 'w' || input == 72) {
		shared_data->y--;
	} else if (input == 's' || input == 80) {
		shared_data->y++;
	} else if (input == 'a' || input == 75) {
		shared_data->x--;
	} else if (input == 'd' || input == 77) {
		shared_data->x++;
	}

	// 避免坐标超出范围
	shared_data->x = (shared_data->x < 0) ? 0 : (shared_data->x > 49) ? 49 : shared_data->x;
	shared_data->y = (shared_data->y < 0) ? 0 : (shared_data->y > 29) ? 29 : shared_data->y;

	// 根据输入的值修改光标位置
	if (shared_data->arr[shared_data->x][shared_data->y] != '*' && shared_data->arr[shared_data->x][shared_data->y] != 'o') {
		shared_data->arr[shared_data->x][shared_data->y] = '+'; // 更新新位置
	}

	// 复原之前位置
	if (shared_data->arr[pastX][pastY] == '+') {
		shared_data->arr[pastX][pastY] = '.'; // 清除光标路径
	}
}

// 函数原型声明
void sendCoordinates(int sock, int x, int y);


// 根据输入下棋
void playChess(int input, int sock) {
	char updateChar = '*';
	int nextPlayer = 1;
	if(PLAYER == 1) {
		updateChar = 'o';
		nextPlayer = 0;
	}
	if (shared_data->judgeWhoPlayChess == PLAYER) {
		if (shared_data->arr[shared_data->x][shared_data->y] != '*' && shared_data->arr[shared_data->x][shared_data->y] != 'o') {
			shared_data->arr[shared_data->x][shared_data->y] = updateChar;
			shared_data->judgeWhoPlayChess = nextPlayer;
			// 发送网络请求通知下棋的位置
			sendCoordinates(sock, shared_data->x, shared_data->y);
		}
	}
}

// 打印棋盘
void printChessboard() {
	for (int j = 0; j < 30; j++) {
		for (int i = 0; i < 50; i++) {
			printf(" %c", shared_data->arr[i][j]);
		}
		printf("\n");
	}
	if(shared_data->judgeWhoPlayChess == 0) {
		printf("轮到正方 * 下棋\n");
	} else {
		printf("轮到反方 o 下棋\n");
	}
}

// 判断是否取得胜利，0为还未胜利 ,1为先手*胜利，2为后手o胜利
// 这里为了防止另一方赢了没被检测，采用遍历，而不是根据最后一手为谁
int judgeVector() {
	int (*arr)[BOARD_COLS] = shared_data->arr;

	// 定义胜利符号
	char player1 = '*';
	char player2 = 'o';

	// 定义检查方向
	int directions[4][2] = {
		{1, 0},   // 横向
		{0, 1},   // 纵向
		{1, 1},   // 从左到右斜线
		{-1, 1}   // 从右到左斜线
	};

	// 遍历整个棋盘
	for (int i = 0; i < BOARD_ROWS; i++) {
		for (int j = 0; j < BOARD_COLS; j++) {
			// 如果当前位置没有棋子，继续
			if (arr[i][j] != player1 && arr[i][j] != player2) continue;

			// 检查每个方向
			for (int d = 0; d < 4; d++) {
				int dx = directions[d][0];
				int dy = directions[d][1];
				int count1 = 0, count2 = 0;

				for (int step = 0; step < 5; step++) {
					int x = i + step * dx;
					int y = j + step * dy;

					if (x < 0 || x >= BOARD_ROWS || y < 0 || y >= BOARD_COLS) break;

					if (arr[x][y] == player1) count1++;
					else count1 = 0;

					if (arr[x][y] == player2) count2++;
					else count2 = 0;

					if (count1 >= 5) return 1; // player1获胜
					if (count2 >= 5) return 2; // player2获胜
				}
			}
		}
	}

	return 0; // 还未胜利
}

// 结束界面
void endInterface() {
	printf("游戏结束\n");
	if (shared_data->judge == 1)
		printf("先手方 * 胜\n");
	else
		printf("后手方 o 胜\n");
}

// 保存棋盘到文件
void saveChessBoard() {
	FILE *file = fopen(FILE_NAME, "a");  // 以附加模式打开文件
	if (file == NULL) {
		perror("无法打开文件");
		return;
	}

	// 获取当前日期和时间
	time_t now = time(NULL);
	struct tm *t = localtime(&now);
	fprintf(file, "日期: %04d-%02d-%02d %02d:%02d:%02d\n",
	        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
	        t->tm_hour, t->tm_min, t->tm_sec);


	// 打印棋盘到文件
	for (int j = 0; j < BOARD_COLS; j++) {
		for (int i = 0; i < BOARD_ROWS; i++) {
			fprintf(file, " %c", shared_data->arr[i][j]);
		}
		fprintf(file, "\n");
	}
	fprintf(file, "\n");
	fclose(file);  // 关闭文件
}

/*
    套接字相关操作
*/

// 创建并连接到服务器的函数
int createAndConnectToServer(const char* ip_address) {
	int sock = 0;
	struct sockaddr_in serv_addr;

	// 创建套接字
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("\nSocket creation error \n");
		return -1;
	}

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(SERVER_PORT);

	// 将IPv4地址从文本转为二进制形式
	if (inet_pton(AF_INET, ip_address, &serv_addr.sin_addr) <= 0) {
		printf("\nInvalid address/ Address not supported \n");
		return -1;
	}

	// 连接到服务器
	if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
		printf("\nConnection Failed \n");
		return -1;
	}

	return sock;  // 返回套接字
}

// 发送坐标
void sendCoordinates(int sock, int x, int y) {
	char buffer[BUFFER_SIZE];
	snprintf(buffer, sizeof(buffer), "%d,%d", x, y);  // 格式化坐标为 "x,y"
	send(sock, buffer, strlen(buffer), 0);
}

// 接收并解析坐标的函数
void receiveCoordinates(int sock) {
	char revieveChess = 'o';
	int nextPlayer = 0;
	if(PLAYER == 1) {
		revieveChess = '*';
		nextPlayer = 1;
	}
	char buffer[BUFFER_SIZE] = {0};
	while (1) {
		int valread = read(sock, buffer, sizeof(buffer) - 1);
		if (valread > 0) {
			buffer[valread] = '\0';  // 确保字符串以空字符结束
			int x, y;
			sscanf(buffer, "%d,%d", &x, &y);  // 解析坐标
			shared_data->arr[x][y]=revieveChess; // 下棋
			shared_data->judgeWhoPlayChess=nextPlayer; // 更换选手
			clearScreen();
			printChessboard();
		}
	}
}

/*
    业务相关大模块(main方法直接调用)
*/

// 主页显示
void homeInterface() {
	clearScreen();
	for (int i = 0; i <= 10; i++)
		printf("\n");
	for (int i = 0; i <= 20; i++)
		printf(" ");
	printf("五子棋联机版\n");
	for (int i = 0; i <= 20; i++)
		printf(" ");
	printf("        -- made by flyingpig\n\n\n\n");

	printf("  0. 退出\n");
	printf("  1. 开始游戏\n");
	printf("  2. 查看历史对局\n");
	printf("  请选择一个选项: \n");
}

// 查看历史对局
void viewLastGameBoard() {
	FILE *file = fopen(FILE_NAME, "r");  // 以读模式打开文件
	if (file == NULL) {
		perror("无法打开文件");
		return;
	}

	char line[256];
	printf("\n\n查看历史对局:\n\n");

	// 逐行读取并打印文件内容
	while (fgets(line, sizeof(line), file)) {
		printf("%s", line);
	}

	fclose(file);  // 关闭文件

	printf("\n\n  0. 返回初始界面\n");
	printf("  1. 开始游戏\n");
	printf("  请选择一个选项: \n");
}

// 下棋流程
int playChessService() {
	// 创建并连接到服务器
	int sock = createAndConnectToServer(SERVER_IP);
	if (sock < 0) {
		return -1;  // 连接失败
	}
	// 创建并初始化共享内存
	createAndInitializeSharedMemory();
	if (shared_data == NULL) {
		return 1; // 处理错误
	}
	// 创建子进程
	pid_t pid = fork();
	switch (pid) {
			// 创建子进程失败
		case -1:
			printf("创建子进程失败\n");
			return -1;

			// 子进程：接收坐标
		case 0:
			receiveCoordinates(sock);
			return 0;

			// 主进程：下棋流程
		default:

			while (!shared_data->judge) {
				clearScreen();
				printChessboard();
				int input = getch();
				// 如果输入是空格或者0，那么就在光标所在位置下棋
				if (input == ' ' || input == '0') {
					playChess(input, sock);
				} else {
					moveCursor(input);
				}
				shared_data->judge = judgeVector();
			}
			endInterface();
			saveChessBoard();
			close(sock); // 关闭套接字
			waitpid(pid, NULL, 0); // 等待子进程结束
			printf("Press Enter to exit...");
			getchar();
			return 0;
	}
}


int main() {
	while (1) {  // 无限循环，直到用户选择退出
		homeInterface();
		switch (getch()) {
			case '0':
				exit(0);
			case '1':
				return playChessService();
			case '2':
				viewLastGameBoard();
				char choice = getch();
				if (choice == '0') {
					continue;  // 返回初始界面，重新循环
				} else if (choice == '1') {
					return playChessService();
				}
				break;
			default:
				break; // 无效选项，重新选择
		}
	}
}