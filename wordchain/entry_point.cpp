/*
* 
* WORDCHAIN For C++
* Made by Lee chan wook
* 
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <winsock2.h>
#include <vector>
#include <windows.h>
#include <map>
#include <algorithm>
#include <thread>

#include <conio.h>
#include <time.h>

#define endl '\n'

// 라이브러리 포함 빌드
#pragma comment(lib, "ws2_32")

// TCP 서버 옵션
#define PORT              16783
#define PACKET_SIZE  4096

// 방향기
#define UP 72
#define DOWN 80
#define RIGHT 77
#define LEFT 75

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define DESIRED_WINSOCK_VERSION 0x0101
#define MINIMUM_WINSOCK_VERSION 0x0001

using namespace std;

char cur[2] = {};
int ind = 0;

vector<string> split(string in, char delimiter) {
	vector<string> rst;
	stringstream ss(in);
	string buffer;
	if (delimiter == '\n') {
		while (getline(ss, buffer))
			rst.push_back(buffer);
	} else {
		while (getline(ss, buffer, delimiter))
			rst.push_back(buffer);
	}

	return rst;
}

vector<string> splitByString(string s, string delimiter) {
	size_t pos = 0;
	std::string buff;
	vector<string> res;
	while ((pos = s.find(delimiter)) != std::string::npos) {
		buff = s.substr(0, pos);
		res.push_back(buff);
		s.erase(0, pos + delimiter.length());
	}
	res.push_back(s);
	return res;
}

namespace util {
	string setw(int x) {
		return string(x, ' ');
	}
	void setConsoleDefault() {
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0xF0);

		// 콘솔 커스텀 폰트
		CONSOLE_FONT_INFOEX cfi;
		cfi.cbSize = sizeof(cfi);
		cfi.nFont = 0;
		cfi.dwFontSize.X = 0; // 폰트 x 간격
		cfi.dwFontSize.Y = 22; // 폰트 y 간격
		cfi.FontFamily = FF_DONTCARE;
		cfi.FontWeight = FW_NORMAL;
		wcscpy_s(cfi.FaceName, L"맑은 고딕 bold"); // 폰트 설정
		SetCurrentConsoleFontEx(GetStdHandle(STD_OUTPUT_HANDLE), FALSE, &cfi);

		RECT r;
		GetWindowRect(GetConsoleWindow(), &r);
		MoveWindow(GetConsoleWindow(), r.left, r.top, 1200, 600, TRUE);

		// 콘솔 스크롤바 방지
		CONSOLE_SCREEN_BUFFER_INFO screenBufferInfo;
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &screenBufferInfo);

		COORD new_screen_buffer_size;
		new_screen_buffer_size.X = screenBufferInfo.srWindow.Right -
			screenBufferInfo.srWindow.Left + 1;
		new_screen_buffer_size.Y = screenBufferInfo.srWindow.Bottom -
			screenBufferInfo.srWindow.Top + 1;

		SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), new_screen_buffer_size);

		// 콘솔 리사이즈 방지
		SetWindowLong(GetConsoleWindow(), GWL_STYLE, GetWindowLong(GetConsoleWindow(), GWL_STYLE) & ~WS_MAXIMIZEBOX & ~WS_SIZEBOX);

		SetConsoleTitle("[ 끝말잇기 by 찬욱 ]");
	}
	void setMenu(int arrow) {
		if (arrow == UP) {
			for (int i = 0; i < sizeof(cur) / sizeof(char); i++) {
				cur[i] = NULL;
			}
			if (ind - 1 >= 0)
				ind = ind - 1;
			cur[ind] = '>';
		}
		if (arrow == DOWN) {
			for (int i = 0; i < sizeof(cur) / sizeof(char); i++) {
				cur[i] = NULL;
			}
			if (ind+ 1 < sizeof(cur) / sizeof(char))
				ind = ind + 1;
			cur[ind] = '>';
		}
	}
}

class Player {
private:
	string name;
	SOCKET socket;
	bool isTurn = false;
public:
	Player(string name) {
		this->name = name;
	}
	Player(string name, SOCKET socket) {
		this->name = name;
		this->socket = socket;
	}

	bool isMyTurn() {
		return this->isTurn;
	}

	void setTurn(bool flag) {
		this->isTurn = flag;
	}

	string getName() {
		return this->name;
	}

	void setSocket(SOCKET s) {
		this->socket = s;
	}

	void send(string cmd) {
		if (this->socket == NULL) return;;
		::send(this->socket, cmd.c_str(), cmd.length(), 0);
	}
	
	SOCKET getSocket() {
		return this->socket;
	}
};

bool isGameStarted = false;
vector<Player*> playerList;
vector<Player*> alivePlayer;
Player* nowPlayer;

class Client {
private:
	SOCKET socket;
	Player* playerClass = NULL;
public:
	Client(SOCKET sock) {
		this->socket = sock;
	}

	static void broadcast(string msg, Player* ignore = NULL) {
		for (int i = 0; i < playerList.size(); i++) {
			if (playerList[i] == ignore) continue;
			playerList[i]->send("broadcast::" + msg);
		}
	}

	void read();

	void send(string cmd) {
		if (socket != NULL) {
			::send(this->socket, cmd.c_str(), cmd.length(), 0);
		}
	}

	void close() {
		closesocket(this->socket);
		this->socket = NULL;
	}

	void start() {
		thread t([&]() {
			this->read();
			});
		t.detach();
	}
};

namespace wordchain {
	vector<string> words;
	map<string, vector<string>> wordmap;
	vector<string> usedWord;

	string chainWord = "";
	string lastUse = "";

	void resetGame() {
		usedWord.clear();
		chainWord = " ";
	}

	string readFile(const string& path) {
		ifstream is(path);
		is.imbue(std::locale(".UTF8"));

		if (!is.is_open())
			return "";

		return string((istreambuf_iterator<char>(is)), istreambuf_iterator<char>());
	}

	bool isNonProtectWord(string str) {
		return wordmap[str.substr(0, 2)].size() == 0 ? false : true;
	}

	bool isRealWord(string str) {
		vector<string> temp = wordmap[str.substr(0, 2)];
		return find(temp.begin(), temp.end(), str) != temp.end();
	}

	vector<string> getWord(string start) {
		return wordmap[start];
	}

	void useWord(string str) {
		usedWord.push_back(str);
	}

	bool isUsedWord(string str) {
		return find(usedWord.begin(), usedWord.end(), str) != usedWord.end();
	}

	void proceed(string word) {
		chainWord = word.substr(word.length() - 2, word.length());
	}

	int _random(int min, int max) {
		int val = max - min;
		if (val == 1 || val == 0)  val = max;
		return rand() % val +min;
	}

	string getStartWord() {
		while (true) {
			int r = _random(0, wordmap.size() - 1);
			auto it = wordmap.begin();
			advance(it, r);

			if (it->second.size() > 50) {
				int random2 = _random(0, it->second.size() - 1);

				return it->second[random2].substr(0, 2);
			}
		}
	}

	vector<string> getWordList(string start) {
		return wordmap[start];
	}

	string getBotWord(string start) {
		vector<string> wordlist = getWordList(start);
		if (wordlist.size() == 0) return "";

		int ran = _random(0, wordlist.size());
		return wordlist[ran];
	}

	string utf82str(string& str) {
		wchar_t arr[256];
		int len = str.size();
		memset(arr, '\0', sizeof(arr));
		MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, arr, 256);

		char carr[256];
		memset(carr, '\0', sizeof(carr));
		WideCharToMultiByte(CP_ACP, 0, arr, -1, carr, 256, NULL, NULL);
		return carr;
	}

	BOOL setup() {
		string w = readFile("db.txt");

		if (w.empty()) {
			cout << "| db 파일이 없습니다." << endl;
			cout << "| db 파일을 현재 폴더에 넣어주세요." << endl;
			return 0;
		}

		cout << "[!] 단어 설정 시작!" << endl;
		words = split(w, '\n');

		for (string& s : words) {
			s = utf82str(s);
			wordmap[s.substr(0, 2)] = vector<string>();
		}

		for (string s : words) {
			vector<string> v = wordmap[s.substr(0, 2)];
			v.push_back(s);
			wordmap[s.substr(0, 2)] = v;
		}

		cout << "[!] 단어 설정 완료!";
		return 1;
	}

	void withBot(BOOL isChain = 0, int next = 0) {
		if (!isChain) {
			system("cls");
			cout << "초보 봇과의 끝말잇기 입니다." << endl;

			next = _random(1, 2);
			chainWord = getStartWord();

			cout << "시작 단어는 [";
			cout << chainWord << "] 입니다." << endl;
			cout << endl;
		}

		if (wordmap[chainWord].size() == 0) { // 쓸 단어가 없을대
			cout << "게임이 끝났습니다." << endl;
			Sleep(3000);
			return;
		}

		if (next == 1) { // 봇 턴
			cout << "봇 차례입니다." << endl;
			string word = getBotWord(chainWord);

			cout << "단어 : [" << word << "]" << endl;
			cout << "차례 : 플레이어" << endl;
			cout << endl;

			useWord(word);
			proceed(word);
			withBot(1, 0);
		} else { // 플레이어 턴
			cout << "플레이어 차례입니다." << endl;
			while (true) {
				string word;
				cout << "단어를 입력해주세요 :  ";
				cin >> word;
				cout << endl;

				if (word.substr(0, 2) != chainWord) {
					cout << "첫 단어가 맞지 않습니다. : [" << word << "] " << endl;
					continue;
				}

				if (isUsedWord(word)) {
					cout << "이미 사용한 단어 : [" << word << "]" << endl;
					continue;
				}
				if (!isRealWord(word)) {
					cout << "존재하지 않는 단어 : [" << word << "]" << endl;
					continue;
				}

				cout << "단어 : [" << word << "]" << endl;
				useWord(word);
				proceed(word);
				withBot(1, 1);
				break;
			}
		}
	}

	/*                         WITH PLAYER                          */

	void withPlayer(BOOL isChain = 0, int next = -1) {
		if (!isChain) {
			if (next == -1) {
				alivePlayer.clear();
				for (Player* it : playerList)
					alivePlayer.push_back(it);
			}
			next = _random(0, alivePlayer.size()+1);

			chainWord = getStartWord();
			cout << "시작 단어는 [" + chainWord + "] 입니다." << endl;
			Client::broadcast("시작 단어는 [" + chainWord + "] 입니다.");
		}

		// SET WINNER
		if (alivePlayer.size() <= 1) {
			cout << "[" + alivePlayer[0]->getName() + "] 님이 우승하셨습니다!" << endl;
			Client::broadcast("[" + alivePlayer[0]->getName() + "] 님이 우승하셨습니다!");
			isGameStarted = false;
			return;
		}

		if (next >= alivePlayer.size()) next = 0;

		cout << endl;
		Client::broadcast("");
		cout << "현재 차례 : [" + alivePlayer[next]->getName() + "]" << endl;
		Client::broadcast("현재 차례 : [" + alivePlayer[next]->getName() + "]");
		cout << "시작 단어는 [" + chainWord + "] 입니다." << endl;
		Client::broadcast("시작 단어는 [" + chainWord + "] 입니다.");
		Client::broadcast("제한 시간 : 10초");
		cout << "제한 시간 : 10초" << endl;

		nowPlayer = alivePlayer[next];
		nowPlayer->setTurn(true);

		time_t now = time(nullptr) * 1000;
		thread t([](time_t time, Player* nowPlayer, int index) {
			while (nowPlayer->isMyTurn()) {
				time_t now = ::time(nullptr);
				time_t mnow = now * 1000;

				if (mnow - time >= 10000) {
					cout << "[" + nowPlayer->getName() + "] 님이 시간 초과로 탈락되었습니다." << endl;
					Client::broadcast("[" + nowPlayer->getName() + "] 님이 시간 초과로 탈락되었습니다.");
					nowPlayer->setTurn(false);
					alivePlayer.erase(alivePlayer.begin() + index);
					break;
				}
				Sleep(1);
			}
		}, now, alivePlayer[next], next);
		t.detach();

		while (nowPlayer->isMyTurn()) {
			Sleep(1);
		}

		if (alivePlayer.size() <= 1) {
			cout << "[" + alivePlayer[0]->getName() + "] 님이 우승하셨습니다!" << endl;
			Client::broadcast("[" + alivePlayer[0]->getName() + "] 님이 우승하셨습니다!");
			isGameStarted = false;
			return;
		}

		cout << "단어 : [" << lastUse << "]" << endl;
		Client::broadcast("단어 : [" + lastUse + "]");

		withPlayer(TRUE, next + 1);
	}
}

void Client::read() {
	while (socket != NULL) {
		char received[256];
		int length = recv(this->socket, received, sizeof(received), 0);

		if (length > 0) {
			received[length] = '\0'; // \0 padding ( 문자열 마지막 표시 )

			string cmd = string(received);

			if (cmd.rfind("join::", 0) == 0) {
				if (isGameStarted) { // 이미 게임이 시작되었다면.
					this->send("broadcast::이미 게임이 시작되었습니다. 다음 게임에 참가해주세요!");
					this->close();
					return;
				}

				string name = splitByString(cmd, "::")[1];
				Player* pl = new Player(name, this->socket);
				playerList.push_back(pl);
				this->playerClass = pl;
				cout << name << "님이 게임에 참가하셨습니다." << endl;
				broadcast(name + "님이 게임에 참가하셨습니다.");
			}
			if (cmd.rfind("chat::", 0) == 0) {
				string name = splitByString(cmd, "::")[1];
				string content = splitByString(cmd, "::")[2];
				if (this->playerClass == nowPlayer) {
					if (content.substr(0, 2) != wordchain::chainWord) {
						send("broadcast::첫 단어가 맞지 않습니다. : [" + content + "] ");
						continue;
					}

					if (wordchain::isUsedWord(content)) {
						send("broadcast::이미 사용한 단어 : [" + content + "] ");
						continue;
					}
					if (!wordchain::isRealWord(content)) {
						send("broadcast::존재하지 않는 단어 : [" + content + "] ");
						continue;
					}
					wordchain::useWord(content);
					wordchain::proceed(content);
					playerClass->setTurn(false);
					wordchain::lastUse = content;
					
					continue;
				}

				cout << name << ": " << content << endl;
				broadcast(name + ": " + content, this->playerClass);
			}
		} else { // 연결이 종료되었을 때
			if (this->playerClass != NULL) {
				if (!isGameStarted) {
					std::remove(playerList.begin(), playerList.end(), this->playerClass);
					std::remove(alivePlayer.begin(), alivePlayer.end(), this->playerClass);
					cout << this->playerClass->getName() << "님이 게임에서 나가셨습니다." << endl;
					broadcast(this->playerClass->getName() + "님이 게임에서 나가셨습니다.");
				}
			}
			this->close();
		}
	}
}

class toServer {
private:
	SOCKET server;
	string nickname;
	bool shutdown = false;
public:
	toServer(SOCKET s, string name) {
		this->server = s;
		this->nickname = name;
	}

	void setShutdown(bool flag) {
		this->shutdown = flag;
	}

	void read() {
		while (server != NULL) {
			char received[256];
			int length = recv(this->server, received, sizeof(received), 0);

			if (length > 0) {
				received[length] = '\0'; // \0 padding ( 문자열 마지막 표시 )

				string cmd = string(received);
				if (cmd.rfind("broadcast::", 0) == 0) {
					vector<string> strings = splitByString(cmd, "broadcast::");
					for (int i = 1; i < strings.size(); i++) {
						cout << strings[i] << endl;
					}
					//cout << splitByString(cmd, "::")[1] << endl;
				}

				if (cmd == "cls")
					system("cls");
			} else { // 연결이 종료되었을 때
				cout << "서버와의 연결이 끊어졌습니다." << endl;
				this->close();
				break;
			}
		}
	}

	void write() {
		while (server != NULL || !shutdown) {
			SOCKET ss = this->server;
			string cmd;
			getline(cin, cmd);
			stringstream(cmd) >> cmd;
			if (server != ss) return;
			if (cmd.empty()) continue;
			if (ss == NULL) { // if user is SERVER
				if (cmd == "start" && !isGameStarted) {
					for (int i = 0; i < playerList.size(); i++) {
						playerList[i]->send("cls");
						playerList[i]->send("broadcast::호스트가 게임을 시작했습니다.");
					}
					isGameStarted = true;
					continue;
				}

				if (playerList[0]->isMyTurn()) { // if it is my turn

					if (cmd.substr(0, 2) != wordchain::chainWord) {
						cout << "첫 단어가 맞지 않습니다. : [" << cmd << "] " << endl;
						continue;
					}

					if (wordchain::isUsedWord(cmd)) {
						cout << "이미 사용한 단어 : [" << cmd << "]" << endl;
						continue;
					}
					if (!wordchain::isRealWord(cmd)) {
						cout << "존재하지 않는 단어 : [" << cmd << "]" << endl;
						continue;
					}
					wordchain::useWord(cmd);
					wordchain::proceed(cmd);
					playerList[0]->setTurn(false);
					wordchain::lastUse = cmd;
					continue;
				}

				for (int i = 0; i < playerList.size(); i++) {
					playerList[i]->send("broadcast::서버: " + cmd);
				}
			} else { // else user is just USER
				send("chat::" + this->nickname + "::" + cmd);
			}
		}
	}

	void send(string cmd) {
		if (socket != NULL) {
			::send(this->server, cmd.c_str(), cmd.length(), 0);
		}
	}

	void close() {
		closesocket(this->server);
		this->server = NULL;
	}

	void start() {
		thread t([&]() {
			this->read();
			});
		t.detach();
	}
	void startInput() {
		thread t2([&]() {
			this->write();
			});
		t2.detach();
	}
};

namespace Socket {
	SOCKET serverSocket;
	SOCKET connection;

	string GetInIpAddress() {
		WSADATA wsadata;
		string strIP = "";

		if (!WSAStartup(DESIRED_WINSOCK_VERSION, &wsadata)) {
			if (wsadata.wVersion >= MINIMUM_WINSOCK_VERSION) {
				HOSTENT* p_host_info;
				IN_ADDR in;
				char host_name[128] = { 0, };

				gethostname(host_name, 128);
				p_host_info = gethostbyname(host_name);

				if (p_host_info != NULL) {
					for (int i = 0; p_host_info->h_addr_list[i]; i++) {
						memcpy(&in, p_host_info->h_addr_list[i], 4);
						strIP = inet_ntoa(in);
					}
				}
			}
			WSACleanup();
		}
		return strIP;
	}

	BOOL join(string ip, int port) {
		WSADATA wsdata;
		BOOL rst = ::WSAStartup(MAKEWORD(0x02, 0x02), &wsdata);
		if (ERROR_SUCCESS != rst)
			return FALSE;

		// 서버에 통신할 소켓 만들기
		Socket::connection = ::socket(PF_INET, SOCK_STREAM, 0);
		if (INVALID_SOCKET == Socket::connection)
			return FALSE;

		// 서버에 연결하기
		SOCKADDR_IN servAddr;
		servAddr.sin_family = AF_INET;
		servAddr.sin_addr.s_addr = inet_addr(ip.c_str());
		servAddr.sin_port = htons(PORT);
		rst = ::connect(Socket::connection, (LPSOCKADDR)&servAddr, sizeof(servAddr));
		if (rst != ERROR_SUCCESS)
			return FALSE;

		return TRUE;
	}
	
	BOOL send(SOCKET server, string content) {
		BOOL rst = ::send(server, content.c_str(), content.length(), 0);
		if (!rst)
			return FALSE;

		return TRUE;
	}

	BOOL listen(int port) {

		// 윈도우 소켓 초기화
		WSADATA wsadata;
		BOOL rst = WSAStartup(MAKEWORD(2, 2), &wsadata);
		if (ERROR_SUCCESS != rst)
			return FALSE;

		// TCP 소켓 생성
		serverSocket = ::socket(PF_INET, SOCK_STREAM, 0);
		if (INVALID_SOCKET == serverSocket)
			return FALSE;

		// IP와 포트를 생성한 소켓에 바인딩함
		SOCKADDR_IN servAddr;
		servAddr.sin_family = AF_INET;
		servAddr.sin_addr.s_addr = inet_addr(GetInIpAddress().c_str());
		servAddr.sin_port = htons(port);
		rst = ::bind(serverSocket, (LPSOCKADDR)&servAddr, sizeof(servAddr));
		if (ERROR_SUCCESS != rst)
			return FALSE;

		// Lienten
		rst = ::listen(serverSocket, SOMAXCONN);
		if (ERROR_SUCCESS != rst)
			return FALSE;

		return TRUE;
	}
}

int main() {
	util::setConsoleDefault();

	srand((unsigned int)time(NULL));

	BOOL rst = wordchain::setup();
	if (!rst) {
		Sleep(3000);
		return 1;
	}

	Sleep(500);

	cur[ind] = '>';
	
	while (true) {
		system("cls");
		cout << " __       __                            __   ______   __                  __           \r\n/  |  _  /  |                          /  | /      \\ /  |                /  |          \r\n$$ | / \\ $$ |  ______    ______    ____$$ |/$$$$$$  |$$ |____    ______  $$/  _______  \r\n$$ |/$  \\$$ | /      \\  /      \\  /    $$ |$$ |  $$/ $$      \\  /      \\ /  |/       \\ \r\n$$ /$$$  $$ |/$$$$$$  |/$$$$$$  |/$$$$$$$ |$$ |      $$$$$$$  | $$$$$$  |$$ |$$$$$$$  |\r\n$$ $$/$$ $$ |$$ |  $$ |$$ |  $$/ $$ |  $$ |$$ |   __ $$ |  $$ | /    $$ |$$ |$$ |  $$ |\r\n$$$$/  $$$$ |$$ \\__$$ |$$ |      $$ \\__$$ |$$ \\__/  |$$ |  $$ |/$$$$$$$ |$$ |$$ |  $$ |\r\n$$$/    $$$ |$$    $$/ $$ |      $$    $$ |$$    $$/ $$ |  $$ |$$    $$ |$$ |$$ |  $$ |\r\n$$/      $$/  $$$$$$/  $$/        $$$$$$$/  $$$$$$/  $$/   $$/  $$$$$$$/ $$/ $$/   $$/ \r\n                                                                                       \r\n                                                                                       \r\n                                                                                       " << endl << endl;

		cout << cur[0] << " | 혼자 하기" << endl;
		cout << endl;
		cout << cur[1] << " | 멀티플레이" << endl;

		int arr = _getch();
		if (arr == 224) {
			int arrow = _getch();
			util::setMenu(arrow);
		}

		// ENTER
		if (arr == 13) {
			if (ind == 0) {
				wordchain::resetGame();
				wordchain::withBot();
			}
			if (ind == 1) {
				system("cls");
				int n;
				cout << util::setw(2) << "① | 서버 열기" << endl;
				cout << util::setw(2) << "② | 서버 참가하기" << endl;
				cin >> n;
				cin.ignore();
				if (n == 1) {
					playerList.clear();
					wordchain::resetGame();
					Socket::listen(PORT);
					system("cls");
					cout << util::setw(2) << "서버를 성공적으로 열었습니다." << endl;
					cout << util::setw(2) << "아이피 : " << Socket::GetInIpAddress() << endl;
					cout << endl;

					cout << util::setw(2) << "플레이어의 참가를 기다리는 중..." << endl;
					cout << util::setw(2) << "시작하려면 start를 입력해주세요!" << endl;
					toServer* ts = new toServer(NULL, "");
					ts->startInput();
					playerList.push_back(new Player("서버"));

					thread t([]() {
						while (!isGameStarted) {
							// ACCEPT
							SOCKET sockAccept = ::accept(Socket::serverSocket, NULL, NULL);
							if (sockAccept != -1) {
								Client* cl = new Client(sockAccept);
								cl->start();
							}
						}
					});
					t.detach();

					while (!isGameStarted) {
						Sleep(1);
					}
					Sleep(100);
					wordchain::withPlayer(0);
					Sleep(2000);

					// 서버소켓 닫기
					BOOL flag = 1;
					for (Player* s : playerList) {
						if (s != NULL)
							closesocket(s->getSocket());
					}
					ts->setShutdown(true);
					cin.clear();
					closesocket(Socket::serverSocket);
				} else if (n == 2) {
					system("cls");
					string nickname;
					cout << "당신이 쓸 닉네임을 입력해주세요!" << endl;
					getline(cin, nickname);

					cout << "IP를 입력해주세요!" << endl;
					string ip;
					getline(cin, ip);

					BOOL rst = Socket::join(ip, PORT);
					if (!rst) {
						cout << "해당 아이피로 열린 서버가 없습니다." << endl;
						Sleep(1000);
					} else {
						toServer* ts = new toServer(Socket::connection, nickname);
						ts->send("join::" + nickname);
						ts->startInput();
						ts->read();
						ts->setShutdown(true);
						delete ts;
					}
				}
			}
		}
	}

	return 0;
}