#pragma once
#ifndef CLIENT_GAME_PLAY
#define CLIENT_GAME_PLAY

#include <winsock2.h>
#include "../shared/common.h"

int play_against_cpu(SOCKET m_socket);
int game_play_results(SOCKET m_socket, RX_msg *rx_msg, char *username);
#endif // !CLIENT_GAME_PLAY