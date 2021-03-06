// This file is part of playd.
// playd is licensed under the MIT licence: see LICENSE.txt.

/**
 * @file player.cpp
 * Main implementation file for the Player class.
 * @see player.hpp
 */

#include <cassert>
#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "audio/audio_system.hpp"
#include "audio/audio.hpp"
#include "cmd_result.hpp"
#include "errors.hpp"
#include "response.hpp"
#include "messages.h"
#include "player.hpp"

const std::vector<std::string> Player::FEATURES{"End", "FileLoad", "PlayStop",
                                                "Seek", "TimeReport"};

Player::Player(AudioSystem &audio)
    : audio(audio), file(audio.Null()), is_running(true), sink(nullptr)
{
}

void Player::SetSink(ResponseSink &sink)
{
	this->sink = &sink;
}

bool Player::Update()
{
	assert(this->file != nullptr);
	auto as = this->file->Update();

	if (as == Audio::State::AT_END) this->End();
	if (as == Audio::State::PLAYING) {
		// Since the audio is currently playing, the position may have
		// advanced since last update.  So we need to update it.
		this->Read("/player/time/elapsed", 0);
	}

	return this->is_running;
}

void Player::WelcomeClient(size_t id) const
{
	this->sink->Respond(Response(Response::Code::OHAI).AddArg(MSG_OHAI), id);

	auto features = Response(Response::Code::FEATURES);
	for (auto &f : FEATURES) features.AddArg(f);
	this->sink->Respond(features, id);

	this->Read("/", id);
}

void Player::End()
{
	this->SetPlaying(false);

	// Rewind the file back to the start.  We can't use Player::Seek() here
	// in case End() is called from Seek(); a seek failure could start an
	// infinite loop.
	this->SeekRaw(0);

	// Let upstream know that the file ended by itself.
	// This is needed for auto-advancing playlists, etc.
	if (this->sink == nullptr) return;
	this->sink->Respond(Response(Response::Code::END));
}

//
// Commands
//

CommandResult Player::RunCommand(const std::vector<std::string> &cmd, size_t id)
{
	if (!this->is_running) {
		// Refuse any and all commands when not running.
		// This is mainly to prevent the internal state from
		// going weird, but seems logical anyway.
		return CommandResult::Failure(MSG_CMD_PLAYER_CLOSING);
	}

	// TODO: more specific message
	if (cmd.size() == 0) return CommandResult::Invalid(MSG_CMD_INVALID);

	auto word = cmd[0];
	auto nargs = cmd.size() - 1;

	// These commands accept one more argument than they use.
	// This is because the first argument is a 'tag', emitted with the
	// command result to allow it to be identified, but otherwise
	// unused.
	if (nargs == 2 && "read" == word) return this->Read(cmd[2], id);
	if (nargs == 2 && "delete" == word) return this->Delete(cmd[2]);
	if (nargs == 3 && "write" == word) return this->Write(cmd[2], cmd[3]);

	return CommandResult::Invalid(MSG_CMD_INVALID);
}

CommandResult Player::Eject()
{
	assert(this->file != nullptr);
	this->file = this->audio.Null();
	this->Read("/control/state", 0);

	return CommandResult::Success();
}

CommandResult Player::Load(const std::string &path)
{
	if (path.empty()) return CommandResult::Invalid(MSG_LOAD_EMPTY_PATH);

	assert(this->file != nullptr);

	// Bin the current file as soon as possible.
	// This ensures that we don't have any situations where two files are
	// contending over resources, or the current file spends a second or
	// two flushing its remaining audio.
	this->file = this->audio.Null();

	try {
		assert(this->file != nullptr);
		this->file = this->audio.Load(path);
		this->Read("/", 0);
		assert(this->file != nullptr);
	} catch (FileError &e) {
		// File errors aren't fatal, so catch them here.
		this->Eject();
		return CommandResult::Failure(e.Message());
	} catch (Error &) {
		// Ensure a load failure doesn't leave a corrupted track
		// loaded.
		this->Eject();
		throw;
	}

	return CommandResult::Success();
}

CommandResult Player::SetPlaying(bool playing)
{
	// Why is SetPlaying not split between Start() and Stop()?, I hear the
	// best practices purists amongst you say.  Quite simply, there is a
	// large amount of fiddly exception boilerplate here that would
	// otherwise be duplicated between the two methods.  The 'public'
	// interface Player gives is Start()/Stop(), anyway.

	assert(this->file != nullptr);

	try {
		this->file->SetPlaying(playing);
	} catch (NoAudioError &e) {
		return CommandResult::Invalid(e.Message());
	}

	this->Read("/control/state", 0);

	return CommandResult::Success();
}

CommandResult Player::Quit()
{
	this->Eject();
	this->is_running = false;
	return CommandResult::Success();
}

CommandResult Player::Seek(const std::string &time_str)
{
	std::uint64_t pos = 0;
	try {
		pos = SeekParse(time_str);
	} catch (SeekError &e) {
		// Seek errors here are a result of clients sending weird times.
		// Thus, we tell them off.
		return CommandResult::Invalid(e.Message());
	}

	try {
		this->SeekRaw(pos);
	} catch (NoAudioError) {
		return CommandResult::Invalid(MSG_CMD_NEEDS_LOADED);
	} catch (SeekError) {
		// Seek failures here are a result of the decoder not liking the
		// seek position (usually because it's outside the audio file!).
		// Thus, unlike above, we try to recover.

		Debug() << "Seek failure" << std::endl;

		// Make it look to the client as if the seek ran off the end of
		// the file.
		this->End();
	}

	// If we've made it all the way down here, we deserve to succeed.
	return CommandResult::Success();
}

/* static */ std::uint64_t Player::SeekParse(const std::string &time_str)
{
	size_t cpos = 0;

	// In previous versions, this used to parse a unit at the end.
	// This was removed for simplicity--use baps3-cli etc. instead.
	std::uint64_t pos;
	try {
		pos = std::stoull(time_str, &cpos);
	} catch (...) {
		throw SeekError(MSG_SEEK_INVALID_VALUE);
	}

	// cpos will point to the first character in pos that wasn't a number.
	// We don't want any such characters here, so bail if the position isn't
	// at the end of the string.
	auto sl = time_str.length();
	if (cpos != sl) throw SeekError(MSG_SEEK_INVALID_VALUE);

	return pos;
}

void Player::SeekRaw(std::uint64_t pos)
{
	assert(this->file != nullptr);

	this->file->Seek(pos);
	this->Read("/player/time/elapsed", 0);
}

// Any resource with the single child "" (empty string) is an entry.
// These need to be looked up via Audio, not handled by Player.
const std::multimap<std::string, std::string> Player::RESOURCES = {
	{"/", "/control"},
	{"/", "/player"},
	{"/control", "/control/state"},
	{"/control/state", ""},
	{"/player", "/player/file"},
	{"/player", "/player/time"},
	{"/player/file", ""},
	{"/player/time", "/player/time/elapsed"},
	{"/player/time/elapsed", ""}
};

CommandResult Player::Read(const std::string &path, size_t id) const
{
	assert(this->file != nullptr);

	// Maybe the requested item is a directory?
	auto count = Player::RESOURCES.count(path);
	if (0 < count) {
		auto range = Player::RESOURCES.equal_range(path);

		// Is this an entry?  If so, delegate it to Audio to work on.
		if (1 == count && "" == range.first->second) {
			// The entry might be currently empty, in which case
			// Emit will return nullptr.  This is fine, but we'll just
			// act as if it doesn't exist at all.
			auto response = this->file->Emit(path, id == 0);
			if (!response) return CommandResult::Failure(MSG_NOT_FOUND);

			if (this->sink != nullptr) this->sink->Respond(*response, id);
			return CommandResult::Success();
		}

		// Otherwise, it's a directory.
		// First, emit the directory resource.
		auto res = Response::Res("Directory", path, std::to_string(count));
		if (this->sink != nullptr) this->sink->Respond(*res, id);

		// Next, the contents.
		for (auto i = range.first; i != range.second; i++) {
			this->Read(i->second, id);
		}

		return CommandResult::Success();
	}

	// If we get here, the resource doesn't exist and never will do.
	return CommandResult::Failure(MSG_NOT_FOUND);
}

CommandResult Player::Write(const std::string &path, const std::string &payload)
{
	if ("/control/state" == path) {
		if ("Playing" == payload) return this->SetPlaying(true);
		if ("Stopped" == payload) return this->SetPlaying(false);
		if ("Ejected" == payload) return this->Eject();
		if ("Quitting" == payload) return this->Quit();
		return CommandResult::Invalid(MSG_INVALID_PAYLOAD);
	}

	if ("/player/file" == path) return this->Load(payload);
	if ("/player/time/elapsed" == path) return this->Seek(payload);

	return this->ResourceFailure(path);
}

CommandResult Player::Delete(const std::string &path)
{
	if ("/control/state" == path) return this->Quit();
	if ("/player/file" == path) return this->Eject();
	if ("/player/time/elapsed" == path) return this->Seek("0");

	return this->ResourceFailure(path);
}

CommandResult Player::ResourceFailure(const std::string &path) {
	// In this case, we've either got a resource that exists but can't
	// be written, or a resource that doesn't.  Let's find out which:
	if (0 < this->RESOURCES.count(path)) {
		// The resource is valid, but can't be written to.
		return CommandResult::Failure(MSG_INVALID_ACTION);
	}

	// Else, the resource doesn't exist.
	return CommandResult::Failure(MSG_NOT_FOUND);
}

