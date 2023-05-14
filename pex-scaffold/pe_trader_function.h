#ifndef PE_TRADER_FUNCTION_H
#define PE_TRADER_FUNCTION_H
#include "pe_common.h"

void teardown();
void deserialize_response(char* buf, Response* response);
void serialize_order(Order* order, char* buf);
void send_message_to_exchange(char* buf);
void handle_exchange_reponse(Response* response);
#endif  // PE_TRADER_FUNCTION_H