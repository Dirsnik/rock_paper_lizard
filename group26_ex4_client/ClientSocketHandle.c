#define _CRT_SECURE_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include "clientsockethandle.h"
#include "ClientGamePlay.h"
#include "../shared/socket_shared.h"
#include "../shared/SocketSendRecvTools.h"
#include "../shared/common.h"


SOCKET m_socket;
expecting_user_input=false;


int failed_connection(const char *server_ip_addr, int port, int flag_type) {
	int decision;
	switch (flag_type) {
	case F_SERVER_CONNECTION_FAILED:
		printf("Failed connecting to server on <%s>:<%d>.\n", server_ip_addr, port);
		break;
	case F_SERVER_CONNECTION_LOST:
		printf("Connection to server on <%s>:<%d> has been lost.\n", server_ip_addr, port);
		break;
	case F_SERVER_DENIED_CONNECTION:
		printf("Server on <%s>:<%d> denied the connection request.\n", server_ip_addr, port);
		break;
	}

	printf("Choose what to do next:\n1. Try to reconnect\n2. Exit\n");
	while (1) {
		scanf("%d", &decision);
		if (decision == 1) { //chooses to try to reconnect
			printf("thanks for choosing 1\n");
			return TRY_TO_RECONNECT;
		}
		else if (decision == 2) { //chooses to exit
			return EXIT_CONNECTION;
		}
		else {
			printf("please enter a valid answer\n");
		}

	}
	return ERR;
}


//Reading data coming from the server
static DWORD RecvDataThread(RX_msg *rx_msg)
{
	TransferResult_t RecvRes;
	int msg_type;
	char *AcceptedStr = NULL;

	RecvRes = ReceiveString(&AcceptedStr, m_socket);

	if (RecvRes == TRNS_FAILED) {
		return (DWORD)F_SERVER_CONNECTION_LOST;
	}
	else if (RecvRes == TRNS_DISCONNECTED) {

		return (DWORD)F_SERVER_CONNECTION_LOST;
	}
	else {
		rx_msg = parse_message_params(AcceptedStr);
		msg_type = rx_msg->msg_type;
		return (DWORD)rx_msg;
		}

	return (DWORD)0;
}

int CreateNewConnectionServer(Flow_param *flow_param) {
	TransferResult_t SendRes;
	HANDLE hThread;
	DWORD wait_code;
	BOOL ret_val;
	RX_msg *rx_msg;
	int client_move;
	char SendStr[256];
	bool connecting = true;

	while (connecting) {
		connecting = false;
		strcpy(SendStr, flow_param->username);
		SendRes = send_msg_one_param(CLIENT_REQUEST, m_socket, flow_param->username);
		if (SendRes == TRNS_FAILED) {
			printf("Socket error while trying to write data to socket\n");
			return ERR_SOCKET;
		}
		/*passing the rx_msg in order to know which type of message we recieve from the server*/
		hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvDataThread, &rx_msg, 0, NULL);

		/*waiting for 15 seconds for response, if the wait time is bigger then assuming the connection is lost*/
		wait_code = WaitForSingleObject(hThread, INFINITE); //RESPONSE time is set to infinite sec NEED TO BE 15!
		if (WAIT_OBJECT_0 != wait_code)
		{
			printf("Waited for 15 seconds, server lost\n");
			TerminateThread(hThread, 0x555);
			return ERR_THREAD_WAIT_TIME;
		}
		GetExitCodeThread(hThread, &rx_msg);
		ret_val = CloseHandle(hThread);
		if (FALSE == ret_val)
		{
			printf("Error when closing thread: %d\n", GetLastError());
			return ERR_CLOSING_THREAD;                        //NEED TO BE CHANGED TO DEFINE
		}
		if (rx_msg->msg_type < 0) {
			client_move = failed_connection(flow_param->ip, flow_param->port, rx_msg->msg_type);
			if (client_move == TRY_TO_RECONNECT) {
				connecting = true;
				continue;
			}
			else {
				return EXIT_CONNECTION;
			}
		}
		//recieved server connected message 
		if (rx_msg->msg_type == SERVER_APPROVED) {
			return APPROVED_BY_SERVER;
		}
	}
	return 0;
}

//Sending data to the server
static DWORD SendDataThread(Flow_param *flow_param)
{
	TransferResult_t SendRes;
	HANDLE hThread;
	DWORD wait_code;
	BOOL ret_val;
	e_Msg_Type msg_rcv = 0, client_menu_select = 0;
	char SendStr[256];
	bool start_connection = true;
	bool threads_are_alive = TRUE;
	RX_msg *rx_msg;

	while (threads_are_alive)
	{
		/*Starting a new connection to the server, the only valid message is server_connected*/
		if (start_connection)
		{
			start_connection = CreateNewConnectionServer(flow_param);
			start_connection = false;
			continue;
		}
		/*if resieved a server connection now waiting for server main menu*/
		else {
			hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)RecvDataThread, &rx_msg, 0, NULL);
			wait_code = WaitForSingleObject(hThread, INFINITE); //RESPONSE time is set to infinite sec NEED TO BE 15!
			if (WAIT_OBJECT_0 != wait_code) {
				printf("Waited for 15 seconds, server lost\n");
				TerminateThread(hThread, 0x555);
			}
			GetExitCodeThread(hThread, &rx_msg);
			if (rx_msg->msg_type < 0) {
				rx_msg->msg_type = failed_connection(flow_param->ip, flow_param->port, rx_msg->msg_type);
			}
			ret_val = CloseHandle(hThread);
			if (FALSE == ret_val) {
				printf("Error when closing thread: %d\n", GetLastError());
				return ERR;                        //NEED TO BE CHANGED TO DEFINE
			}
			if (rx_msg->msg_type == SERVER_MAIN_MENU) {
				//first closing the reading thread				
				client_menu_select = ClientMainMenu(m_socket);
				printf("client_menu_select is %d\n", client_menu_select);
				if (client_menu_select == CLIENT_DISCONNECT) {
					threads_are_alive = FALSE;
					return (DWORD)0;
				}
				threads_are_alive = TRUE;
				continue;									 //waiting for response from the server
			}
			if (rx_msg->msg_type == SERVER_INVITE) {					 //recieved a message and then waiting for another message
				printf("waiting for the server reply\n");
				continue;
			}

			if (rx_msg->msg_type == SERVER_PLAYER_MOVE_REQUEST) {	//recieved a massage and waiting for a replay
				printf("Hi, I'm playing against someone\n");
				msg_rcv = play_against_cpu(m_socket);
				threads_are_alive = TRUE;
				continue;
			}

			if (rx_msg->msg_type == SERVER_GAME_RESULTS) {
				printf("I recieved the results woohooo\n");
				msg_rcv = game_play_results(m_socket, rx_msg, flow_param->username);
				threads_are_alive = TRUE;
				continue;
			}

			if (rx_msg->msg_type == SERVER_GAME_OVER_MENU) {
				printf("I recieved game over menu\n");
				client_menu_select = ClientGameOverMenu(m_socket);
				threads_are_alive = TRUE;
				continue;
			}

		}
	}
	Sleep(1000);
	return (DWORD)0;
}


int MainClient(const char *server_ip_addr, int server_port, char *username) {
	SOCKADDR_IN clientService;
	Flow_param flow_param;
	DWORD wait_code;
	HANDLE hThread;
	int err;
	// Initialize Winsock.
	WSADATA wsaData; //Create a WSADATA object called wsaData.
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != NO_ERROR)
		printf("Error at WSAStartup()\n");

	//Initializing the struct 
	flow_param.ip = server_ip_addr;
	flow_param.port = server_port;
	flow_param.username = username;
	printf("the username is:%s\n", flow_param.username);
	// Create a socket.
	m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check for errors to ensure that the socket is a valid socket.
	if (m_socket == INVALID_SOCKET) {
		printf("Error at socket(): %ld\n", WSAGetLastError());
		WSACleanup();
		return ERR_SOCKET;
	}
	clientService.sin_family = AF_INET;
	clientService.sin_addr.s_addr = inet_addr(server_ip_addr); //Setting the IP address to connect to
	clientService.sin_port = htons(server_port);			 //Setting the port to connect to.
	// Check for general errors.
	if (connect(m_socket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
		err = failed_connection(server_ip_addr, server_port, F_SERVER_CONNECTION_FAILED);
		goto out;		/*Thats not an error: user chose to exit*/
		}
	printf("Connected to server on %s:%d\n", server_ip_addr, server_port);
	hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)SendDataThread, &flow_param, 0, NULL);
	wait_code = WaitForSingleObject(hThread, INFINITE);
	GetExitCodeThread(hThread, &err);
	CloseHandle(hThread);

out:
	closesocket(m_socket);
	WSACleanup();

	return 0;
}
