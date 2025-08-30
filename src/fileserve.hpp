#pragma once

#include <solanaceae/contact/fwd.hpp>
#include <solanaceae/message3/registry_message_model.hpp> // meh

#include <string>
#include <string_view>
#include <vector>

// fwd
struct ConfigModelI;
class MessageCommandDispatcher;
class RegistryMessageModelI;

struct FileServe {
	ConfigModelI& _conf;
	MessageCommandDispatcher& _mcd;
	RegistryMessageModelI& _rmm;
	ContactStore4I& _cs;

	struct FileEntry {
		std::string path; // without filename
		std::string filename;
		int64_t size {0};
	};
	std::vector<FileEntry> _file_list;

	public:
		FileServe(
			ConfigModelI& conf,
			MessageCommandDispatcher& mcd,
			RegistryMessageModelI& rmm,
			ContactStore4I& cs
		);

		// scans dirs specified in config
		bool scanDirs(void);

	protected:
		// adds dir and scans files
		// TODO: async
		// TODO: filter
		// for recursion, to preserve prefix as part of the path/name
		bool addDir(std::string_view dir_path, std::string_view prefix = {});

		bool addEntry(FileEntry&& entry);

		bool sendID(Contact4 from, Contact4 to, std::string_view params);

	protected: // commands
		bool comList(std::string_view params, Message3Handle m);
		bool comGet(std::string_view params, Message3Handle m);
		bool comPost(std::string_view params, Message3Handle m);
		bool comSearch(std::string_view params, Message3Handle m);
		bool comRescan(std::string_view params, Message3Handle m);
};

