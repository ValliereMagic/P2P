#include "Peer.hpp"
#include <iostream>
#include <sstream>
extern "C" {
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
}
namespace ApplicationLayer
{
// Function to insert a filename string into the PeerMessage Header

// :return: false when the filename is too long
bool Peer::set_filename(const std::string &filename, PeerMessage &out_message)
{
	// Make sure that the filename is within the size limits
	if (filename.size() > 254) {
		return false;
	}
	// The message_type is at index 0. We need to start writing this filename at
	// index 1.
	size_t idx = 1;
	for (auto &c : filename) {
		out_message[idx] = c;
		++idx;
	}
	// Set the null terminator
	out_message[idx] = '\0';
	return true;
}

// Function to pull the filename string from the PeerMessage Header
void Peer::pull_filename(std::string &out_filename, const PeerMessage &message)
{
	// The message_type is at index 0. We need to start reading this filename at
	// index 1.
	out_filename.clear();
	for (size_t idx = 1; idx < 256; ++idx) {
		if (message[idx] != '\0')
			out_filename.insert(out_filename.end(), message[idx]);
		else
			break;
	}
}

// Function to read a chunk in from a file

// :return: (whether we were successful, the chunk, the size of the chunk)
std::tuple<bool, std::vector<uint8_t>, uint32_t>
Peer::read_chunk(const std::string &filename, const uint32_t chunk_idx)
{
	// the file from disk, and send it in this message.
	// Open the file to send a chunk of
	int chunk_fid = open(filename.c_str(), O_RDONLY, 0444);
	// Make sure we were able to open the file
	if (chunk_fid < 0) {
		std::cerr << "Error. Unable to read from file being shared.\n";
		return std::make_tuple(false, std::vector<uint8_t>(), 0);
	}
	// Seek to the beginning of the chunk to send
	if (lseek(chunk_fid, chunk_idx * CHUNK_SIZE, SEEK_SET) == -1) {
		std::cerr << "Error. chunk index out of range.\n";
		close(chunk_fid);
		return std::make_tuple(false, std::vector<uint8_t>(), 0);
	}
	// Chunk of the file to send to the other peer.
	std::vector<uint8_t> chunk(CHUNK_SIZE);
	// If this chunk is the end chunk of the file, then it may be less than
	// CHUNK_SIZE
	uint32_t actual_chunk_size;
	if ((actual_chunk_size = read(chunk_fid, chunk.data(), CHUNK_SIZE)) <
	    1) {
		std::cerr << "Unable to read chunk from send file.\n";
		close(chunk_fid);
		return std::make_tuple(false, std::vector<uint8_t>(), 0);
	}
	// Close the file. We are done reading.
	close(chunk_fid);
	// Return back our shiny new chunk.
	return std::make_tuple(true, std::move(chunk), actual_chunk_size);
}
// Build a peer message to send to another peer. File name can be no longer than
// 255 bytes. Do not add the extra null terminator. This function will take care
// of that.

// :return: false when the filename is too long
bool Peer::serialize_message_header(PeerMessage &out_message,
				    const uint8_t message_type,
				    const std::string &file_name,
				    const uint32_t num_chunks,
				    const uint32_t chunk_request_begin_idx,
				    const uint32_t chunk_request_end_idx,
				    const uint32_t current_chunk_idx,
				    const uint32_t current_chunk_size)
{
	// Clear the header
	out_message.fill(0);
	// Message type will always be set.
	out_message[0] = message_type;
	// If the filename is set, put it in the header
	if (file_name != "") {
		if (!set_filename(file_name, out_message))
			return false;
	}
	// Place the other fields in the header
	(*((uint32_t *)&(out_message[256]))) = htonl(num_chunks);
	(*((uint32_t *)&(out_message[260]))) = htonl(chunk_request_begin_idx);
	(*((uint32_t *)&(out_message[264]))) = htonl(chunk_request_end_idx);
	(*((uint32_t *)&(out_message[268]))) = htonl(current_chunk_idx);
	(*((uint32_t *)&(out_message[272]))) = htonl(current_chunk_size);
	return true;
}

// Deconstruct a Peer message into its parts.

// :return: false on failure
void Peer::deserialize_message_header(const PeerMessage &message,
				      uint8_t &out_message_type,
				      std::string &out_file_name,
				      uint32_t &out_num_chunks,
				      uint32_t &out_chunk_request_begin_idx,
				      uint32_t &out_chunk_request_end_idx,
				      uint32_t &out_current_chunk_idx,
				      uint32_t &out_current_chunk_size)
{
	// Pull the fields
	out_message_type = message[0];
	pull_filename(out_file_name, message);
	out_num_chunks = ntohl((*((uint32_t *)&(message[256]))));
	out_chunk_request_begin_idx = ntohl(*((uint32_t *)&(message[260])));
	out_chunk_request_end_idx = ntohl(*((uint32_t *)&(message[264])));
	out_current_chunk_idx = ntohl(*((uint32_t *)&(message[268])));
	out_current_chunk_size = ntohl(*((uint32_t *)&(message[272])));
}

template <typename T>
bool Peer::send_or_recv_socket(int sock, T &container, size_t len,
			       ssize_t (*sock_func)(int sock, void *buff,
						    size_t len))
{
	// A lot of pain was solved by reading this...
	// https://stackoverflow.com/questions/14399691/send-not-deliver-all-bytes
	uint32_t bytes_left = len;
	uint8_t *read_ptr = container.data();
	while (bytes_left > 0) {
		// Try to read/write it all in at once (unlikely)
		ssize_t bytes_done = sock_func(sock, read_ptr, bytes_left);
		if (bytes_done < 0) {
			std::cout << "Unable to send or receive on socket.\n";
			return false;
		} else if (bytes_done == 0) {
			std::cout << "Socket Disconnected.\n";
			return false;
		}
		// Decrement the amount of bytes we still have to read/write in, and
		// move our pointer ahead the number of bytes we have read/written.
		bytes_left -= bytes_done;
		read_ptr += bytes_done;
	}
	return true;
}

// Returns the pieces of the message extracted from the header, as well as
// writes the chunk to the chunks folder passed if this is a CHUNK_RESPONSE
// message. Where the filename is the chunk index number.p2p

// :return: false on failure
bool Peer::read_message(const int socket, uint8_t &out_message_type,
			std::string &out_file_name, uint32_t &out_num_chunks,
			uint32_t &out_chunk_request_begin_idx,
			uint32_t &out_chunk_request_end_idx,
			uint32_t &out_current_chunk_idx,
			uint32_t &out_current_chunk_size, const bool listener,
			const std::string &temp_chunk_dir)
{
	PeerMessage m;
	// Read in the header
	if (!send_or_recv_socket(socket, m, m.size(), read)) {
		return false;
	}
	deserialize_message_header(m, out_message_type, out_file_name,
				   out_num_chunks, out_chunk_request_begin_idx,
				   out_chunk_request_end_idx,
				   out_current_chunk_idx,
				   out_current_chunk_size);
	// Check if this is a Chunk response message and make sure we are not a
	// seeder.
	if (out_message_type == PeerMessageType::CHUNK_RESPONSE && !listener) {
		// Chunk buffer
		std::vector<uint8_t> chunk(CHUNK_SIZE);
		// A lot of pain was solved by reading this...
		// https://stackoverflow.com/questions/14399691/send-not-deliver-all-bytes
		// read in the chunk
		if (!send_or_recv_socket(socket, chunk, out_current_chunk_size,
					 read)) {
			return false;
		}
		// Build the chunk filename
		std::stringstream filename;
		filename << temp_chunk_dir << out_current_chunk_idx
			 << FILE_EXTENSION;
		// Open the temp file for the chunk
		int chunk_fid = open(filename.str().c_str(),
				     O_CREAT | O_WRONLY | O_TRUNC, 0644);
		// Make sure we were able to open the file.
		if (chunk_fid < 0) {
			std::cerr
				<< "Error. Unable to write chunk to temp chunk file.\n";
			return false;
		}
		// Write it to the temp file
		if (write(chunk_fid, chunk.data(), out_current_chunk_size) <
		    out_current_chunk_size) {
			std::cerr
				<< "Error while writing chunk to temp file.\n";
			close(chunk_fid);
			return false;
		}
		close(chunk_fid);
	}
	return true;
}

// Write the message to the client socket passed. If this is a Chunk Response
// message then the corresponding chunk of the file will also be sent to the
// client as the message payload.

// :return: false on failure
bool Peer::write_message(const int socket, const uint8_t message_type,
			 const std::string &file_name,
			 const uint32_t num_chunks,
			 const uint32_t chunk_request_begin_idx,
			 const uint32_t chunk_request_end_idx,
			 const uint32_t current_chunk_idx,
			 const std::string &filename_to_send)
{
	PeerMessage m;
	if (message_type != PeerMessageType::CHUNK_RESPONSE) {
		// Build the header
		if (!serialize_message_header(
			    m, message_type, file_name, num_chunks,
			    chunk_request_begin_idx, chunk_request_end_idx,
			    current_chunk_idx)) {
			std::cerr
				<< "Error. Unable to serialize message header.\n";
			return false;
		}
		// Send the header
		if (!send_or_recv_socket(socket, m, m.size(),
					 (ssize_t(*)(int sock, void *buff,
						     size_t len))write)) {
			return false;
		}
	} else {
		// If the message_type is a CHUNK_RESPONSE we need to read a chunk of
		// the file from disk, and send it in this message.
		auto chunk_tuple =
			read_chunk(filename_to_send, current_chunk_idx);
		bool success = std::get<0>(chunk_tuple);
		// We were unable to successfully read in the chunk of the file
		if (!success)
			return false;
		std::vector<uint8_t> chunk(std::move(std::get<1>(chunk_tuple)));
		uint32_t actual_chunk_size = std::get<2>(chunk_tuple);
		// Build the header
		if (!serialize_message_header(
			    m, message_type, file_name, num_chunks,
			    chunk_request_begin_idx, chunk_request_end_idx,
			    current_chunk_idx, actual_chunk_size)) {
			std::cerr
				<< "Error. Unable to serialize message header.\n";
			return false;
		}
		// Send the header
		if (!send_or_recv_socket(socket, m, m.size(),
					 (ssize_t(*)(int sock, void *buff,
						     size_t len))write)) {
			return false;
		}
		// Send the chunk to the peer
		if (!send_or_recv_socket(socket, chunk, actual_chunk_size,
					 (ssize_t(*)(int sock, void *buff,
						     size_t len))write)) {
			return false;
		}
	}
	return true;
}
} // namespace ApplicationLayer
