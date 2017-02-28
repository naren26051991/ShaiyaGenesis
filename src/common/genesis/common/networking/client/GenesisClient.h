#ifndef GENESIS_COMMON_NETWORKING_CLIENT_GENESISCLIENT_H
#define GENESIS_COMMON_NETWORKING_CLIENT_GENESISCLIENT_H

#include <string>
#include <iostream>
#include <mutex>

#include "../packets/Packet.h"
#include "../packets/PacketBuilder.h"

#include <boost/asio.hpp>
#include <boost/bind.hpp>

#define MAX_PACKET_LENGTH 1024

namespace Genesis::Common::Networking::Client {

	class GenesisClient {

		public:

			GenesisClient(boost::asio::io_service &io_service) 
				: io_service(io_service), socket(io_service) {}

			bool connect(std::string address, unsigned short port);

			void handle_read(unsigned char* data, const boost::system::error_code &error, unsigned int bytes_read);

			/**
			 * Writes a packet to this session's socket
			 *
			 * @param packet
			 *		The packet instance to write
			 */
			void write(Genesis::Common::Networking::Packets::Packet* packet, std::function<void(unsigned char*, unsigned int)> callback) {

				// The length of the packet
				unsigned int length = packet->length + 4;

				// The array to write to
				unsigned char* data = new unsigned char[length];

				// The request id
				unsigned int request_id = (unsigned int) rand();

				// Lock the mutex
				this->mutex.lock();

				// Generate a unique request id
				while (this->request_map.count(request_id) != 0)
					request_id = (unsigned int) rand();
				
				// Store the request
				this->request_map[request_id] = callback;

				// Unlock the mutex
				this->mutex.unlock();
				
				// Write the length
				data[0] = (length);
				data[1] = (length >> 8);

				// Write the opcode
				data[2] = (packet->opcode);
				data[3] = (packet->opcode >> 8);

				// Write the request id
				data[4] = (request_id);
				data[5] = (request_id >> 8);
				data[6] = (request_id >> 16);
				data[7] = (request_id >> 24);

				// Write the data
				for (int i = 0; i < packet->payload.size(); i++) {
					data[8 + i] = packet->payload.at(i);
				}

				// Write the data
				boost::asio::async_write(this->socket, boost::asio::buffer(data, length),
					[&](const boost::system::error_code& error, unsigned int bytes_written) {

						// If an error occurred, close the socket if it is still open
						if (error && this->socket.is_open()) {
							this->socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
							this->socket.close();
						}
				});
	
				// Delete the data array and packet
				delete[] data;
				delete packet;
			}
			
			/**
			 * Defines the function to be called whenever a new packet is received
			 *
			 * @param receive_function
			 *		The function to call
			 */
			void on_receive(std::function<void(unsigned char*, unsigned int)> receive_function) {
				this->receive_function = receive_function;
			}

		private:

			// The mutex
			std::mutex mutex;

			// A map containing the request ids, and their callbacks
			std::map<unsigned int, std::function<void(unsigned char*, unsigned int)>> request_map;

			// The on_receive lambda function, which is called whenever a new packet is received
			std::function<void(unsigned char*, unsigned int)> receive_function;

			// The boost io_service instance
			boost::asio::io_service &io_service;

			// The socket instance
			boost::asio::ip::tcp::socket socket;

			// The array that the client will receive data to
			unsigned char data[1024];
	};
}
#endif