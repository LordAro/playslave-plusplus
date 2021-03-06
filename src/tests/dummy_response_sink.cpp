// This file is part of playd.
// playd is licensed under the MIT licence: see LICENSE.txt.

/**
 * @file
 * Implementation of DummyResponseSink.
 */

#include <ostream>
#include <string>
#include "dummy_response_sink.hpp"

DummyResponseSink::DummyResponseSink(std::ostream &os) : os(os)
{
}

void DummyResponseSink::Respond(const Response &response, size_t) const
{
	this->os << response.Pack() << std::endl;
}

