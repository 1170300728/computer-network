#include <stdio.h>
#include <Windows.h>
#include <process.h>
#include <string.h>
#include <tchar.h>
#include <map>
#include <set>
#include<iostream>
#include<string>
#include <fstream>
#pragma comment(lib,"Ws2_32.lib")
#define MAXSIZE 65507 //�������ݱ��ĵ���󳤶�
#define HTTP_PORT 80 //http �������˿�

using namespace std;

//Http ��Ҫͷ������
struct HttpHeader {
	char method[4]; // POST ���� GET��ע����ЩΪ CONNECT����ʵ���ݲ�����
	char url[1024]; // ����� url
	char host[1024]; // Ŀ������
	char cookie[1024 * 10]; //cookie
	HttpHeader() {
		ZeroMemory(this, sizeof(HttpHeader));
	}
};

BOOL InitSocket();
void ParseHttpHead(char *buffer, HttpHeader * httpHeader);
BOOL ConnectToServer(SOCKET *serverSocket, char *host);
unsigned int __stdcall ProxyThread(LPVOID lpParameter);
BOOL GetFromFile(char* buffer, char* urlname);
BOOL WriteIntoFile(char *buffer,  char *hostname);
BOOL MakeDateIfNeed(char* buffer, char* urlname);
string DeleteFromString(string str, string delstr);

//������ز���
SOCKET ProxyServer;
sockaddr_in ProxyServerAddr;
const int ProxyPort = 10240;
std::map<std::string, std::string> FishWebs = { {"www.4399.com","today.hit.edu.cn"} ,{} };
std::set<std::string> FilteredWebs = {"www.7k7k.com",""};
std::set<std::string> FilteredIPs = { "204.204.204.204","" };

//�����µ����Ӷ�ʹ�����߳̽��д������̵߳�Ƶ���Ĵ����������ر��˷���Դ
//����ʹ���̳߳ؼ�����߷�����Ч��
//const int ProxyThreadMaxNum = 20;
//HANDLE ProxyThreadHandle[ProxyThreadMaxNum] = {0};
//DWORD ProxyThreadDW[ProxyThreadMaxNum] = {0};
struct ProxyParam {
	SOCKET clientSocket;
	SOCKET serverSocket;
};


int _tmain(int argc, _TCHAR* argv[])
{
	printf("�����������������\n");
	printf("��ʼ��...\n");
	if (!InitSocket()) {
		printf("socket ��ʼ��ʧ��\n");
		return -1;
	}

	printf("����������������У������˿� %d\n", ProxyPort);
	SOCKET acceptSocket = INVALID_SOCKET;
	ProxyParam *lpProxyParam;
	HANDLE hThread;

	//������������ϼ���
	while (true) {
		SOCKADDR_IN acceptAddr;
		acceptSocket = accept(ProxyServer, (SOCKADDR*)&acceptAddr, NULL);
		lpProxyParam = new ProxyParam;
		char clientIP[16];

		//if (lpProxyParam == NULL) {
			//continue;
		//}

		lpProxyParam->clientSocket = acceptSocket;
		memcpy(clientIP, inet_ntoa(acceptAddr.sin_addr), 16);
		//�û�IP����
		if (FilteredIPs.find(clientIP) != FilteredIPs.end()) {
			printf("IP������\n");
			closesocket(acceptSocket);
		}
		hThread = (HANDLE)_beginthreadex(NULL, 0, &ProxyThread, (LPVOID)lpProxyParam, 0, 0);
		CloseHandle(hThread);
		Sleep(200);
	}

	closesocket(ProxyServer);
	WSACleanup();
	return 0;
}

//************************************
// Method: InitSocket
// FullName: InitSocket
// Access: public
// Returns: BOOL
// Qualifier: ��ʼ���׽���
//************************************
BOOL InitSocket() {
	//�����׽��ֿ⣨���룩
	WORD wVersionRequested;
	WSADATA wsaData;
	//�׽��ּ���ʱ������ʾ
	int err;
	//�汾 2.2
	wVersionRequested = MAKEWORD(2, 2);
	//���� dll �ļ� Scoket ��
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		//�Ҳ��� winsock.dll
		printf("���� winsock ʧ�ܣ��������Ϊ: %d\n", WSAGetLastError());
		return FALSE;
	}
	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
	{
		printf("�����ҵ���ȷ�� winsock �汾\n");
		WSACleanup();
		return FALSE;
	}
	ProxyServer = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == ProxyServer) {
		printf("�����׽���ʧ�ܣ��������Ϊ��%d\n", WSAGetLastError());
		return FALSE;
	}
	ProxyServerAddr.sin_family = AF_INET;
	ProxyServerAddr.sin_port = htons(ProxyPort);
	ProxyServerAddr.sin_addr.S_un.S_addr = INADDR_ANY;
	if (bind(ProxyServer, (SOCKADDR*)&ProxyServerAddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		printf("���׽���ʧ��\n");
		return FALSE;
	}
	if (listen(ProxyServer, SOMAXCONN) == SOCKET_ERROR) {
		printf("�����˿�%d ʧ��", ProxyPort);
		return FALSE;
	}
	return TRUE;
}

//************************************
// Method: ProxyThread
// FullName: ProxyThread
// Access: public
// Returns: unsigned int __stdcall
// Qualifier: �߳�ִ�к���
// Parameter: LPVOID lpParameter
//************************************
unsigned int __stdcall ProxyThread(LPVOID lpParameter) {
	char Buffer[MAXSIZE];
	char *CacheBuffer;
	ZeroMemory(Buffer, MAXSIZE);
	int length = sizeof(SOCKADDR_IN);
	int recvSize;
	int ret;
	recvSize = recv(((ProxyParam*)lpParameter)->clientSocket, Buffer, MAXSIZE, 0);
	if (recvSize <= 0) {
		goto error;
	}
	{
		HttpHeader* httpHeader = new HttpHeader();
		CacheBuffer = new char[recvSize + 1];
		ZeroMemory(CacheBuffer, recvSize + 1);
		memcpy(CacheBuffer, Buffer, recvSize);
		ParseHttpHead(CacheBuffer, httpHeader);
		delete CacheBuffer;

		//printf("httpHealper:%s\n", Buffer);
		//ʵ����վ���ˣ����������ĳЩ��վ
		if (FilteredWebs.find(httpHeader->host) != FilteredWebs.end()) {
			printf("��������� %s \n", httpHeader->host);
			goto error;
		}

		//ʵ�ַ���������ģ����վ
		if (FishWebs.find(httpHeader->host) != FishWebs.end()) {
			std::string target = FishWebs[httpHeader->host];
			const char * target_c = target.c_str();
			std::string Buffer_s = std::string(Buffer);
			while (Buffer_s.find(std::string(httpHeader->host)) != std::string::npos) {
				int l = Buffer_s.find(std::string(httpHeader->host));
				Buffer_s = Buffer_s.substr(0, l) + target + Buffer_s.substr(l + std::string(httpHeader->host).length());
			}
			memcpy(Buffer, Buffer_s.c_str(), Buffer_s.length() + 1);
			memcpy(httpHeader->host, target_c, target.length());
			printf("���ʵ�������վ���������� %s \n", httpHeader->host);
		}

		if (!ConnectToServer(&((ProxyParam*)lpParameter)->serverSocket, httpHeader->host)) {
			goto error;
		}
		printf("������������ %s �ɹ�\n", httpHeader->host);
		BOOL made = MakeDateIfNeed(Buffer, httpHeader->url);
		//cout << Buffer << endl;
		//���ͻ��˷��͵� HTTP ���ݱ���ֱ��ת����Ŀ�������
		ret = send(((ProxyParam *)lpParameter)->serverSocket, Buffer, strlen(Buffer) + 1, 0);
		//�ȴ�Ŀ���������������
		recvSize = recv(((ProxyParam*)lpParameter)->serverSocket, Buffer, MAXSIZE, 0);
		if (recvSize <= 0) {
			goto error;
		}
		if (made) {
			string buffer_str = Buffer;
			string delim = "\r\n";
			buffer_str = buffer_str.substr(0, 15);

			if (buffer_str.find("200") != buffer_str.npos) {
				WriteIntoFile(Buffer, httpHeader->url);
			}
			else if (buffer_str.find("304") != buffer_str.npos)
			{
				cout << "hello304" << endl;
				GetFromFile(Buffer, httpHeader->url);
			}
		}
		else {
			string buffer_str = Buffer;
			string delim = "\r\n";
			buffer_str = buffer_str.substr(0, 15);

			if (buffer_str.find("200") != buffer_str.npos) {
				WriteIntoFile(Buffer, httpHeader->url);
			}

		}
		//printf("\n������ %s \n", Buffer);
		//��Ŀ����������ص�����ֱ��ת�����ͻ���
		ret = send(((ProxyParam*)lpParameter)->clientSocket, Buffer, sizeof(Buffer), 0);
		//������
	}
error:
	printf("�ر��׽���\n");
	Sleep(200);
	closesocket(((ProxyParam*)lpParameter)->clientSocket);
	closesocket(((ProxyParam*)lpParameter)->serverSocket);
	delete lpParameter;
	_endthreadex(0);
	return 0;
}

//************************************
// Method: ParseHttpHead
// FullName: ParseHttpHead
// Access: public
// Returns: void
// Qualifier: ���� TCP �����е� HTTP ͷ��
// Parameter: char * buffer
// Parameter: HttpHeader * httpHeader
//************************************
void ParseHttpHead(char *buffer, HttpHeader * httpHeader) {
	char *p;
	char *ptr;
	const char * delim = "\r\n";
	p = strtok_s(buffer, delim, &ptr);//��ȡ��һ��
	//printf("%s\n", p);
	if (p[0] == 'G') {//GET ��ʽ
		memcpy(httpHeader->method, "GET", 3);
		memcpy(httpHeader->url, &p[4], strlen(p) - 13);
	}
	else if (p[0] == 'P') {//POST ��ʽ
		memcpy(httpHeader->method, "POST", 4);
		memcpy(httpHeader->url, &p[5], strlen(p) - 14);
	}
	//printf("%s\n", httpHeader->url);
	p = strtok_s(NULL, delim, &ptr);
	while (p) {
		switch (p[0]) {
		case 'H'://Host
			memcpy(httpHeader->host, &p[6], strlen(p) - 6);
			break;
		case 'C'://Cookie
			if (strlen(p) > 8) {
				char header[8];
				ZeroMemory(header, sizeof(header));
				memcpy(header, p, 6);
				if (!strcmp(header, "Cookie")) {
					memcpy(httpHeader->cookie, &p[8], strlen(p) - 8);
				}
			}
			break;
		default:
			break;
		}
		p = strtok_s(NULL, delim, &ptr);
	}
}

//************************************
// Method: ConnectToServer
// FullName: ConnectToServer
// Access: public
// Returns: BOOL
// Qualifier: ������������Ŀ��������׽��֣�������
// Parameter: SOCKET * serverSocket
// Parameter: char * host
//************************************
BOOL ConnectToServer(SOCKET *serverSocket, char *host) {
	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(HTTP_PORT);
	HOSTENT *hostent = gethostbyname(host);
	if (!hostent) {
		return FALSE;
	}
	in_addr Inaddr = *((in_addr*)*hostent->h_addr_list);
	serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(Inaddr));
	*serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (*serverSocket == INVALID_SOCKET) {
		return FALSE;
	}
	if (connect(*serverSocket, (SOCKADDR *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
		closesocket(*serverSocket);
		return FALSE;
	}
	return TRUE;
}

//************************************
// Method: WriteIntoFile
// FullName: WriteIntoFile
// Access: public
// Returns: BOOL
// Qualifier: ����url�����ļ�����������Ҫ����ı��ı�����ļ���
// Parameter: char *buffer
// Parameter: char *urlname
//************************************
BOOL WriteIntoFile(char *buffer, char *urlname) {
	FILE *out;
	std::string filename = urlname;
	//ȥ��url�еķ��ţ�����ʣ�ಿ�ֹ��컺���ļ���
	filename = DeleteFromString(filename, ".");
	filename = DeleteFromString(filename, ":");
	filename = DeleteFromString(filename, "/");
	const char*file = filename.c_str();
	//cout << file <<"+"<<strlen(file)<< endl;
	//cout << buffer << endl;
	//д���ļ�
	if ((out = fopen(file, "w")) != NULL) {
		fwrite(buffer, sizeof(char), strlen(buffer), out);
		fclose(out);
	}
	else
	{
		return false;
	}
	printf("\n�����ѻ��棡\n");
	return true;
}

//************************************
// Method: GetFromFile
// FullName: GetFromFile
// Access: public
// Returns: BOOL
// Qualifier: ����url�����ļ������������ж�Ӧ�ļ����򽫻���ı��������buffer��
// Parameter: char *buffer
// Parameter: char *urlname
//************************************
BOOL GetFromFile(char* buffer, char* urlname) {
	FILE *in;
	std::string filename = urlname;
	//ȥ��url�еķ��ţ�����ʣ�ಿ�ֹ��컺���ļ���
	filename = DeleteFromString(filename, ".");
	filename = DeleteFromString(filename, ":");
	filename = DeleteFromString(filename, "/");
	const char*file = filename.c_str();
	cout << "���յ�304����ȡ��������\n" << endl;
	//���ļ��������buffer
	if ((in = fopen(file, "r")) != NULL) {
		fread(buffer, sizeof(char), MAXSIZE, in);
		fclose(in);
	}
	else {
		cout << file << endl;
		return false;
	}
	return true;
}

//************************************
// Method: MakeDateIfNeed
// FullName: MakeDateIfNeed
// Access: public
// Returns: BOOL
// Qualifier: ����url�����ļ������������ж�Ӧ�ļ���˵���л��棬��ʱ�����µı��������buffer�У�������true
// Parameter: char *buffer
// Parameter: char *urlname
//************************************
BOOL MakeDateIfNeed(char* buffer, char* urlname) {
	FILE *in;
	std::string filename = urlname;
	//ȥ��url�еķ��ţ�����ʣ�ಿ�ֹ��컺���ļ���
	filename = DeleteFromString(filename, ".");
	filename = DeleteFromString(filename, ":");
	filename = DeleteFromString(filename, "/");
	const char*file = filename.c_str();
	if ((in = fopen(file, "r")) != NULL) {
		char fileBuffer[MAXSIZE];
		char *p, tempDate[30], *ptr;
		const char *delim = "\r\n";
		fread(fileBuffer, sizeof(char), MAXSIZE, in);
		p = strtok_s(fileBuffer, delim, &ptr);
		//����Ѱ�ҵ�Date����ȡʱ������
		while (p) {
			if (strstr(p, "Date") != NULL) {
				memcpy(tempDate, &p[6], strlen(p) - 6);
				break;
			}
			p = strtok_s(NULL, delim, &ptr);
		}
		//����ҵ�ʱ�����ݣ�����If-Mosified-Since�����뱨��
		if (tempDate != NULL) {
			string temp = buffer, date = tempDate;
			int ifstart;
			//cout << "temp=" << temp << endl;
			/*ifstart = temp.find("If-Modi");
			if (ifstart != temp.npos) {
				temp.erase(ifstart, 50);
			}*/
			cout << "temp=" << temp << endl;
			string temp1 = temp.substr(0, temp.length() - 4);
			temp = temp1 + "\r\nIf-Modified-Since:" + date.substr(0, 29) + temp.substr(temp.length() - 4, temp.length());
			cout << "temp=" << temp << endl;
			memcpy(buffer, temp.c_str(), temp.length());
			fclose(in);
			return true;
		}
		else
		{
			fclose(in);
			return false;
		}
	}
	else
	{
		return false;
	}
}

//************************************
// Method: DeleteFromString
// FullName: DeleteFromString
// Access: public
// Returns: string
// Qualifier: ���ַ���str���޳����е����ַ���delstr
// Parameter: string str
// Parameter: string delstr
//************************************
string DeleteFromString(string str, string delstr) {
	int it = 0;
	int lenth = delstr.length();
	string newstr = str;
	while (it >= 0) {
		it = newstr.find(delstr);
		if (it >= 0) {
			newstr.erase(it, lenth);
		}
		else
		{
			break;
		}
	}
	return newstr;
}