#include "./fileserve.hpp"

#include <solanaceae/contact/components.hpp>
#include <solanaceae/contact/contact_store_i.hpp>

#include <solanaceae/message3/message_command_dispatcher.hpp>
#include <solanaceae/message3/components.hpp>

#include <solanaceae/util/config_model.hpp>

#include <levenshtein.h>

#include "./string_view_split.hpp"

// TODO: move human readable to util
//#include <solanaceae/util/

#include <filesystem>
#include <string>
#include <charconv>
#include <map>

#include <iostream>
#include <string_view>

FileServe::FileServe(
	ConfigModelI& conf,
	MessageCommandDispatcher& mcd,
	RegistryMessageModelI& rmm,
	ContactStore4I& cs
) : _conf(conf), _mcd(mcd), _rmm(rmm), _cs(cs)
{
	scanDirs();

	_mcd.registerCommand(
		"FileServe", "fserve",
		"list",
		[this](std::string_view params, Message3Handle m) -> bool { return comList(params, m); },
		"List served files",
		MessageCommandDispatcher::Perms::EVERYONE
	);

	_mcd.registerCommand(
		"FileServe", "fserve",
		"get",
		[this](std::string_view params, Message3Handle m) -> bool { return comGet(params, m); },
		"Get file by id (use list/search)",
		MessageCommandDispatcher::Perms::EVERYONE
	);

	_mcd.registerCommand(
		"FileServe", "fserve",
		"post",
		[this](std::string_view params, Message3Handle m) -> bool { return comPost(params, m); },
		"Post file by id. If in group, this will be visible by everyone.",
		//MessageCommandDispatcher::Perms::MODERATOR
		MessageCommandDispatcher::Perms::EVERYONE // TODO: fix moderator
	);

	_mcd.registerCommand(
		"FileServe", "fserve",
		"search",
		[this](std::string_view params, Message3Handle m) -> bool { return comSearch(params, m); },
		"Seach for file matching <string>.",
		MessageCommandDispatcher::Perms::EVERYONE
	);

	_mcd.registerCommand(
		"FileServe", "fserve",
		"rescan",
		[this](std::string_view params, Message3Handle m) -> bool { return comRescan(params, m); },
		"Rescan configured directories.",
		MessageCommandDispatcher::Perms::ADMIN
	);
}

bool FileServe::scanDirs(void) {
	bool succ = true;
	for (const auto [dir_path, enable] : _conf.entries_bool("FileServe", "dirs")) {
		if (enable) {
			succ = succ && addDir(dir_path);
		}
	}

	return succ;
}

bool FileServe::addDir(std::string_view dir_path, std::string_view prefix) {
	try {
		// TODO: maybe make absolute in case wd changes?
		const std::filesystem::path dir{std::filesystem::canonical(dir_path) / prefix};
		if (!std::filesystem::exists(dir)) {
			std::cerr << "FServe: error, path does not exist '" << dir_path << "' + '" << prefix << "'\n";
			return false;
		}

		if (!std::filesystem::is_directory(dir)) {
			std::cerr << "FServe: error, path not a directory '" << dir_path << "' + '" << prefix << "'\n";
			return false;
		}

		std::cout << "FServe: scanning '" << reinterpret_cast<const char*>(dir.generic_u8string().c_str()) << "'\n";
		// TODO: thread this
		for (auto const& dir_entry : std::filesystem::directory_iterator(dir)) {
			try {
				const auto& filepath = dir_entry.path();
				const auto& filename = filepath.filename().generic_u8string();

				if (dir_entry.is_directory()) {
					std::string sub_prefix{prefix};
					if (!sub_prefix.empty()) {
						sub_prefix += "/";
					}
					sub_prefix += reinterpret_cast<const char*>(filename.c_str());

					//addDir(dir_entry.path().generic_u8string(), sub_prefix);
					addDir(dir_path, sub_prefix);

					continue;
				}

				if (!dir_entry.is_regular_file()) {
					continue;
				}

				if (filename.empty()) {
					continue;
				}

				if (filename.at(0) == '.') {
					continue; // skip hidden files
				}

				const auto file_size = static_cast<int64_t>(dir_entry.file_size());
				if (file_size == 0) {
					continue; // skip empty files
				}

				// TODO: check read perm?

				FileEntry fe{
					std::string{dir_path},
					(prefix.empty() ? "" : std::string{prefix} + "/") + reinterpret_cast<const char*>(filename.c_str()),
					file_size
				};
				addEntry(std::move(fe));
			} catch (...) {
				// moving filesystem might throw
				continue;
			}
		}
	} catch (const std::filesystem::filesystem_error& e) {
		std::cerr << "FServe: fs exception thrown for '" << dir_path << "' with " << e.what() << "\n";
		return false;
	} catch (...) {
		std::cerr << "FServe: exception thrown for '" << dir_path << "'\n";
		return false;
	}
	return true;
}

bool FileServe::addEntry(FileEntry&& entry) {
	// find dupe

	for (const auto& [path, name, size] : _file_list) {
		// TODO: more sofistication
		if (name == entry.filename) {
			// TODO: maybe log
			return false;
		}
	}
	std::cout << " + " << entry.filename << "\n";

	_file_list.emplace_back(entry);

	return true;
}

bool FileServe::sendID(Contact4 from, Contact4 to, std::string_view params) {
	int64_t value {0};

	const auto fc_res = std::from_chars(params.data(), params.data()+params.size(), value);
	if (fc_res.ec != std::errc{}) {
		// TODO: log warning
		_rmm.sendText(
			from,
			"error: invalid id (ec:" + std::make_error_code(fc_res.ec).message() + ")"
		);
		return true;
	}

	if (value < 0) {
		// TODO: impl special stuff?
		_rmm.sendText(
			from,
			"error: invalid id (negative)"
		);
		return true;
	}

	if (_file_list.size() <= size_t(value)) {
		// invalid id
		// TODO: log warning
		_rmm.sendText(
			from,
			"error: invalid id"
		);
		return true;
	}

	const auto& requested_file = _file_list.at(value);

	// trim prefix
	std::string_view only_filename = requested_file.filename;
	if (const auto pos = only_filename.find_last_of("/\\"); pos != only_filename.npos) {
		if (pos + 1 >= only_filename.size()) {
			// what
			return true;
		}
		only_filename = only_filename.substr(pos + 1);
	}

	// TODO: error check
	_rmm.sendFilePath(
		to,
		only_filename, // TODO: dont trim and make clients support folders in name
		std::string(requested_file.path) + "/" + requested_file.filename
	);

	return true;
}

bool FileServe::comList(std::string_view params, Message3Handle m) {
	const auto contact_from = m.get<Message::Components::ContactFrom>().c;

	_rmm.sendText(
		contact_from,
		"id - filesize - filename:"
	);

	// TODO: paged

	std::string next_message;
	next_message.reserve(1101);
	for (size_t i = 0; i < _file_list.size(); i++) {
		const auto& entry = _file_list.at(i);
		std::string new_line;
		new_line.reserve(20 + entry.filename.size());

		new_line += std::to_string(i) + " - ";
		// TODO: human readable
		new_line += std::to_string(entry.size) + " - ";
		new_line += "\"" + entry.filename + "\"";

		// TODO: catch abnormal filename sizes

		if (next_message.empty()) {
			next_message = new_line;
		} else if (next_message.size() + 1 + new_line.size() > 1100) {
			_rmm.sendText(
				contact_from,
				next_message
			);
			next_message = new_line;
		} else {
			next_message += "\n" + new_line;
		}
	}

	if (!next_message.empty()) {
		_rmm.sendText(
			contact_from,
			next_message
		);
	}

	return true;
}

bool FileServe::comGet(std::string_view params, Message3Handle m) {
	const auto contact_from = m.get<Message::Components::ContactFrom>().c;

	return sendID(contact_from, contact_from, params);
}

bool FileServe::comPost(std::string_view params, Message3Handle m) {
	const auto contact_from = m.get<Message::Components::ContactFrom>().c;
	const auto contact_to = _cs.contactHandle(m.get<Message::Components::ContactTo>().c);
	if (contact_to.any_of<Contact::Components::TagSelfWeak, Contact::Components::TagSelfStrong>()) {
		// private -> equivalent to get
		return sendID(contact_from, contact_from, params);
	} else {
		return sendID(contact_from, contact_to, params);
	}
}

bool FileServe::comSearch(std::string_view params, Message3Handle m) {
	const auto contact_from = m.get<Message::Components::ContactFrom>().c;

	if (params.empty()) {
		_rmm.sendText(
			contact_from,
			"error: missing search term"
		);
		return true;
	}

	std::multimap<size_t, size_t> dist_to_file;
	for (size_t i = 0; i < _file_list.size(); i++) {
		const auto& entry = _file_list.at(i);

		size_t dist_sum = 0;
		for (const auto sub_term : MM::std_utils::split(params, " \t-_")) {
			if (sub_term.empty()) { // is this possible?
				continue;
			}

			if (entry.filename.size() <= sub_term.size()) {
				dist_sum += levenshtein_n(entry.filename.c_str(), entry.filename.size(), sub_term.data(), sub_term.size());
			} else {
				// move window of search term size over filename, and remember lowest edit distance
				size_t min_dist = 1000u;
				for (size_t j = 0; j+params.size() < entry.filename.size(); j++) {
					const size_t dist = levenshtein_n(entry.filename.c_str()+j, sub_term.size(), sub_term.data(), sub_term.size());
					if (dist == 0) {
						min_dist = 0;
						break;
					}
					if (dist < min_dist) {
						min_dist = dist;
					}
				}
				dist_sum += min_dist;
			}
		}
		dist_to_file.insert({
			dist_sum,
			i
		});
	}

	_rmm.sendText(
		contact_from,
		"id - dist - filesize - filename:"
	);

	std::string next_message;
	next_message.reserve(1101);
	{
		auto it = dist_to_file.cbegin();
		size_t i = 0;
		for (; i < 10 && it != dist_to_file.cend(); i++, it++) {
			const auto dist = it->first;
			const auto file_index = it->second;
			const auto& file_entry = _file_list.at(file_index);

			std::string new_line;
			new_line.reserve(20 + file_entry.filename.size());

			new_line += std::to_string(file_index) + " - ";
			new_line += std::to_string(dist) + " - ";

			// TODO: human readable
			new_line += std::to_string(file_entry.size) + " - ";
			new_line += "\"" + file_entry.filename + "\"";

			if (next_message.empty()) {
				next_message = new_line;
			} else if (next_message.size() + 1 + new_line.size() > 1100) {
				_rmm.sendText(
					contact_from,
					next_message
				);
				next_message = new_line;
			} else {
				next_message += "\n" + new_line;
			}
		}
	}

	if (!next_message.empty()) {
		_rmm.sendText(
			contact_from,
			next_message
		);
	}

	return true;
}

bool FileServe::comRescan(std::string_view, Message3Handle m) {
	const auto contact_from = m.get<Message::Components::ContactFrom>().c;
	_rmm.sendText(
		contact_from,
		"starting rescan"
	);
	_file_list.clear();
	scanDirs();
	return true;
}

