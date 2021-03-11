#include "Swarm.hpp"
extern "C" {
#include <unistd.h>
}

namespace ApplicationLayer
{
// add the byte representation of the passed address and port (which already
// need to be in network byte order) to the passed SwarmMessage
void Swarm::build_message(const uint8_t mode, const uint32_t addr,
			  const uint16_t port, SwarmMessage &message)
{
	// Add the mode
	message[0] = mode;
	// Add the address
	for (uint8_t i = 1; i < 5; ++i) {
		message[i] = ((uint8_t *)(&addr))[i - 1];
	}
	// Add the port
	message[5] = ((uint8_t *)(&port))[0];
	message[6] = ((uint8_t *)(&port))[1];
}

// Read in a message from the client, consisting of a mode, address, and port
// from client_socket passed, and write it to each of the out parameters passed.
bool Swarm::read_message(const int client_socket, uint8_t &out_mode,
			 uint32_t &out_address, uint16_t &out_port,
			 SwarmMessage &out_client_pair)
{
	// Read it in from the client
	if (read(client_socket, out_client_pair.data(),
		 out_client_pair.size()) < (ssize_t)(out_client_pair.size())) {
		return false;
	}
	// Pull the mode from the message
	out_mode = out_client_pair[0];
	// Pull the address and port
	out_address = *((uint32_t *)(&out_client_pair[1]));
	out_port = *((uint16_t *)(&out_client_pair[5]));
	return true;
}

// Write message to passed client
bool Swarm::write_message(const int client_socket,
			  const SwarmMessage &client_pair)
{
	if (write(client_socket, client_pair.data(), client_pair.size()) <
	    (ssize_t)(client_pair.size())) {
		return false;
	}
	return true;
}
} // namespace ApplicationLayer
