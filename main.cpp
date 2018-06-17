#pragma comment(lib,"Ws2_32.lib")
#include <sys/types.h>
#include <WinSock2.h>
#include <Ws2tcpip.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <sstream>


#define HASH_K 5 // Смещение по Цезарю
#define MSG_LEN 512 // Длина сообщения

class Client // Класс клиента
{
private:
	SOCKET socket; // сокет
	std::string user_name; // имя
public:
	Client(SOCKET * in, std::string _name) :socket(*in), user_name(_name) {}
	Client() {}
	SOCKET GetSock() { return socket; }
	std::string GetName() { return user_name; }
	friend bool operator==(const Client& c1, const Client& c2)
	{
		return c1.socket == c2.socket;
	}
	friend bool operator!=(const Client& c1, const Client& c2)
	{
		return !(c1 == c2);
	}
};

SOCKET Connect; // Сокет подключений
SOCKET Listen; // Сокет прослушки
int Count = 0; // Количество действующих клиентов

std::vector<Client> Connection; // Все клиенты


void ReSendForAll(Client* source, std::string msg) // Отправить всем
{
	msg.resize(MSG_LEN); // Установка длины сообщения
	for (auto i = Connection.begin(); i != Connection.end(); i++) // обход всех в векторе и отправка им сообщений
		if (*i != *source && i->GetSock() != INVALID_SOCKET)
			send(i->GetSock(), msg.c_str(), MSG_LEN, NULL);
}

void(*SendAllPtr)(Client* source, std::string) = &ReSendForAll; // указатель на функцию выше

void SystemSend(Client* client) // Система отправки
{
	for (;; Sleep(75)) // Бесконечный цикл
	{
		char msg[MSG_LEN]; // сообщения
		ZeroMemory(msg, sizeof(char) * MSG_LEN); // очистка памяти
		int iResult = recv(client->GetSock(), msg, MSG_LEN, NULL); // получение сообщения
		std::cout << msg << std::endl; // вывод в консоль
		if (iResult == SOCKET_ERROR) // если ошибочный сокет
		{
			for (auto i = Connection.begin(); i != Connection.end(); i++) // Убрать его из вектора
				if (*i == *client) {
					Connection.erase(i);
					break;
				}
			std::string msgOut = "User " + client->GetName() + " out"; // Сообщение выхода
			SendAllPtr(client, msgOut); // Отправить всем
			std::cout << msgOut << std::endl;
			closesocket(client->GetSock()); // Закрыть его
			delete client; // удалить клиента
			Count--;
			break;
			return;
		}
		else
			SendAllPtr(client, std::string(msg)); // Отправить всем
	}
	std::cout << "Close connect\n";
}

void(*SystemSendPtr)(Client* source) = &SystemSend; // Указатель на функцию выше

std::string password = "aaaa"; // пароль

std::string GetHash(std::string input) // Получить хэш пароля через смещение
{
	std::transform(input.begin(), input.end(), input.begin(), [](char c) {return (c + HASH_K) % 255; });
	return input;
}

int main()
{
	{
		std::cout << "Creating server:\n";
		WSAData  ws; // Установка настройки соединения
		WORD version = MAKEWORD(2, 2); // Установка версии
		int MasterSocket = WSAStartup(version, &ws); // Запуск
		if (MasterSocket != 0)
		{
			return EXIT_FAILURE;
		}
		struct addrinfo hints; // Стуктуры под хранение информации о подключении
		struct addrinfo * result;
		ZeroMemory(&hints, sizeof(hints)); // Очистка памяти под 0

		// Установка настроек и технологий
		hints.ai_family = AF_INET;
		hints.ai_flags = AI_PASSIVE;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		// Ввод адреса и порта
		std::cout << "Enter ip:\n";
		std::string iport; std::cin >> iport;
		std::cout << "Enter port:\n";
		std::string port; std::cin >> port;
		getaddrinfo(iport.c_str(), port.c_str(), &hints, &result);

		// Настройка сокета прослушки
		Listen = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
		bind(Listen, result->ai_addr, result->ai_addrlen);
		listen(Listen, SOMAXCONN);
		freeaddrinfo(result);
	}

	std::cout << "Start" << std::endl;
	std::string c_connect = "Connect";
	while (true) // Бесконечный цикл
	{
		Connect = accept(Listen, NULL, NULL); // Ожидание подключения
		if (Connect != INVALID_SOCKET) // Если сокет рабочий
		{
			char userPass[15]; // Проверка пароля пользователя
			int res = recv(Connect, userPass, 15, NULL);
			if (res == SOCKET_ERROR)
				continue;
			// Ответ клиенту
			char msgAccess[1] = { GetHash(password) == GetHash(std::string(userPass)) ? '1' : '0' };
			send(Connect, msgAccess, 1, NULL);
			if (msgAccess[0] == '0')
			{
				std::cout << "Invalid Socket\n";
				closesocket(Connect);
				continue;
			}
			char fmsg[100]; // Ожидание сообщения приглашения
			ZeroMemory(fmsg, 100);
			res = recv(Connect, fmsg, 100, NULL);
			if (res == SOCKET_ERROR)
				continue;
			// Извлечение имени пользователя
			std::stringstream ss(fmsg);
			std::string name;
			ss >> name >> name;
			if (name.length() == 0)
			{
				closesocket(Connect);
				continue;
			}
			std::cout << c_connect << ' ' << Count << std::endl;
			Client * client = new Client(&Connect, name); // Создание клиента
			Connection.push_back(*client); // Добавление его в вектор
			SendAllPtr(client, std::string(fmsg)); // Отправка всем сообщения приглашения
			std::thread th(*SystemSendPtr, client); // Создание потока
			th.detach(); // Отделение потока
			Count++;
		}
	}
	return 0;
}
