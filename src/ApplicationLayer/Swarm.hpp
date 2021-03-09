#pragma once
#include <array>
namespace ApplicationLayer
{
// Message Definition
using SwarmMessage = std::array<uint8_t, 1 + 4 + 2>;
enum SwarmMessageType { REMOVE = 0, ADD = 1 };

class Swarm {
    public:
	// add the byte representation of the passed address and port (which already
	// need to be in network byte order) to the passed SwarmMessage
	static void build_message(const uint8_t mode, const uint32_t addr,
				  const uint16_t port, SwarmMessage &message);

	// Read in a Swarm Message, consisting of a mode, address, and port from
	// socket passed, and write it to each of the out parameters passed.
	// :return: Whether read was successful
	static bool read_message(const int socket, uint8_t &out_mode,
				 uint32_t &out_address, uint16_t &out_port,
				 SwarmMessage &out_client_pair);

	// Write message to passed client
	static bool write_message(const int socket,
				  const SwarmMessage &client_pair);
};
} // namespace ApplicationLayer