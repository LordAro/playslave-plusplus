/*
 * This file is part of Playslave-C++.
 * Playslave-C++ is licenced under MIT License. See LICENSE.txt for more
 * details.
 */

#include <memory>
#include <vector>

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include <algorithm>
#include <thread>
#include <chrono>

#ifdef WIN32
struct timespec {
	time_t tv_sec;
	long tv_nsec;
};
#include <Winsock2.h>
#else
#include <time.h> /* struct timespec */
#endif

#include "cmd.hpp"
#include "io.hpp"

#include "audio.hpp"
#include "constants.h"
#include "messages.h"
#include "player.hpp"

Player::Player(const std::string &device) : device(device)
{
	this->current_state = State::EJECTED;
	this->au = nullptr;

	this->position_listener = nullptr;
	this->position_period = std::chrono::microseconds(0);
	this->position_last = std::chrono::microseconds(0);
	this->position_last_invalid = true;
}

State Player::CurrentState()
{
	return this->current_state;
}

void Player::Update()
{
	if (this->current_state == State::PLAYING) {
		if (this->au->IsHalted()) {
			Eject();
		} else {
			SendPositionIfReady();
		}
	}
	if (CurrentStateIn({State::PLAYING, State::STOPPED})) {
		if (!this->au->Update()) {
			Eject();
		}
	}
}

bool Player::IfCurrentStateIn(Player::StateList states, std::function<void()> f)
{
	return IfCurrentStateIn(states, [f] {
		f();
		return true;
	});
}

bool Player::IfCurrentStateIn(Player::StateList states, std::function<bool()> f)
{
	bool result = false;

	if (CurrentStateIn(states)) {
		result = f();
	}

	return result;
}

bool Player::CurrentStateIn(Player::StateList states)
{
	return std::find(states.begin(), states.end(), this->current_state) !=
	       states.end();
}

void Player::SetState(State state)
{
	State last_state = this->current_state;

	this->current_state = state;

	if (this->state_listener != nullptr) {
		this->state_listener(last_state, state);
	}
}

void Player::RegisterStateListener(StateListener listener)
{
	this->state_listener = listener;
}

void Player::SendPositionIfReady()
{
	auto position = this->au->CurrentPosition<std::chrono::microseconds>();
	if (IsReadyToSendPosition(position)) {
		this->position_listener(position);
		this->position_last = position;
		this->position_last_invalid = false;
	}
}

bool Player::IsReadyToSendPosition(std::chrono::microseconds current_position)
{
	bool ready = false;

	if (this->position_last_invalid) {
		ready = true;
	} else if (this->position_listener != nullptr) {
		auto difference = current_position - this->position_last;
		ready = difference >= this->position_period;
	}

	return ready;
}

void Player::RegisterPositionListener(PositionListener listener,
                                      const std::chrono::microseconds period)
{
	this->position_listener = listener;
	this->position_period = period;
}
