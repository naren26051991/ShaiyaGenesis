#include "GenesisClient.h"
#include "../../logging/Logger.h"

#include <thread>
#include <chrono>
#include <iostream>
#include <iomanip>

bool Genesis::Common::Networking::Client::GenesisClient::connect(std::string address, unsigned short port) {

	// Attempt to catch any exceptions thrown
	try {

		// Seed the random number generator
		srand(time(NULL));

		// The port string
		std::string port_string = std::to_string(port);

		// The tcp resolver instance
		boost::asio::ip::tcp::resolver resolver(this->io_service);

		// The resolver query, to find the server to connect to
		boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(), address, port_string);

		// The query iterator
		boost::asio::ip::tcp::resolver::iterator iterator = resolver.resolve(query);

		// The endpoint
		boost::asio::ip::tcp::resolver::iterator end;

		// The system error
		boost::system::error_code error = boost::asio::error::host_not_found;

		// Loop until no error occurs
		while (error && iterator != end) {
			this->socket.close();
			this->socket.connect(*iterator++, error);
		}
		
		// If we still have an error, return false
		if (error) {
			return false;
		}

		// Begin reading some data
		this->socket.async_read_some(boost::asio::buffer(this->data, MAX_PACKET_LENGTH), boost::bind(&Genesis::Common::Networking::Client::GenesisClient::handle_read, this, this->data, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));

		// Run the io_service instance
		this->io_service.run();
		
		// Return if the socket is connected
		return this->socket.is_open();

	// Catch an exception
	} catch (std::exception &e) {
		genesis_logger->error(e.what());
	}

	// Return false if an error occurred
	return false;
}

void Genesis::Common::Networking::Client::GenesisClient::handle_read(unsigned char* data, const boost::system::error_code &error, unsigned int bytes_read) {
	
	// If an error occurred, close the connection
	if (error) {
		if (this->socket.is_open()) {
			this->socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
			this->socket.close();
		}

		// Log the error
		genesis_logger->error("Error occured in GenesisClient! Aborting process...");
		exit(1);
	}
	
	// The packet length
	unsigned int packet_length = ((data[0] & 0xFF) + ((data[1] & 0xFF) << 8));
	
	// The packet opcode
	unsigned short packet_opcode = ((data[2] & 0xFF) + ((data[3] & 0xFF) << 8));

	// The request id
	unsigned int request_id = ((data[4] & 0xFF) + ((data[5] & 0xFF) << 8) + ((data[6] & 0xFF) << 16) + ((data[7] & 0xFF) << 24));

	// The packet data
	unsigned char* packet_data = (data + 8);

	// Lock the mutex
	this->mutex.lock();

	// Call the receive function
	if (this->request_map.count(request_id) != 0) {

		// Define the callback
		auto callback = request_map[request_id];

		// Erase the request
		request_map.erase(request_id);

		// Call the callback
		callback(packet_data, packet_length - 8);
	}

	// Unlock the mutex
	this->mutex.unlock();
	
	// Begin reading some data
	this->socket.async_read_some(boost::asio::buffer(this->data, MAX_PACKET_LENGTH), boost::bind(&Genesis::Common::Networking::Client::GenesisClient::handle_read, this, this->data, boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
}