// This file is part of playd.
// playd is licensed under the MIT licence: see LICENSE.txt.

/**
 * @file
 * Main entry point and implementation of the playd class.
 * @see main.cpp
 */

#include <algorithm>
#include <iostream>

#include "audio/audio_system.hpp"
#include "io/io_core.hpp"
#include "io/io_response.hpp"
#include "player/player.hpp"
#include "cmd.hpp"
#include "messages.h"
#include "time_parser.hpp"

#include "main.hpp"

/**
 * The main entry point.
 * @param argc Program argument count.
 * @param argv Program argument vector.
 * @return The exit code (zero for success; non-zero otherwise).
 */
int main(int argc, char *argv[])
{
	Playd ps(argc, argv);
	return ps.Run();
}

//
// Playd
//

const TimeParser::MicrosecondPosition Playd::POSITION_PERIOD(500000);

/// ID returned by GetDeviceID when no device ID could be found.
const int Playd::INVALID_ID = -1;


int Playd::GetDeviceID()
{
	// Did the user provide an ID at all?
	if (this->arguments.size() < 2) return INVALID_ID;

	// Only accept valid numbers (stoi will throw for invalid ones).
	int id;
	try {
		id = std::stoi(this->arguments[1]);
	}
	catch (...) {
		// Only std::{invalid_argument, std::out_of_range} are thrown here.
		return INVALID_ID;
	}

	// Only allow valid, outputtable devices; reject input-only devices.
	if (!this->audio.IsOutputDevice(id)) return INVALID_ID;

	return id;
}

Playd::Playd(int argc, char *argv[])
    : audio(), player(audio, time_parser), handler(player), time_parser()
{
	for (int i = 0; i < argc; i++) {
		this->arguments.push_back(std::string(argv[i]));
	}

	auto size = this->arguments.size();

	std::string addr = size > 2 ? this->arguments.at(2) : "0.0.0.0";
	std::string port = size > 3 ? this->arguments.at(3) : "1350";
	this->io = decltype(this->io)(
	                new IoCore(this->player, this->handler, addr, port));
}

int Playd::Run()
{
	try {
		int id = this->GetDeviceID();
		if (id == INVALID_ID) {
			// Show the user the valid device IDs they can use.
			auto device_list = this->audio.GetDevicesInfo();
			for (const auto &device : device_list) {
				std::cout << device.first << ": "
				          << device.second << std::endl;
			}
			return EXIT_FAILURE;
		}
		this->audio.SetDeviceID(id);

		this->player.SetPositionResponsePeriod(POSITION_PERIOD);
		this->player.SetResponseSink(*this->io);
		this->io->Run();
	}
	catch (Error &error) {
		io->Respond(ResponseCode::FAIL, error.Message());
		Debug() << "Unhandled exception caught, going away now."
		        << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
