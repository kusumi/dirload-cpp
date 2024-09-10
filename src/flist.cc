#include <iostream>
#include <fstream>
#include <filesystem>
#include <algorithm>

#include "./flist.h"
#include "./util.h"

std::vector<std::string> init_flist(const std::string& input, bool ignore_dot) {
	std::vector<std::string> l;
	for (const auto& x : std::filesystem::recursive_directory_iterator(input)) {
		auto f = x.path();
		auto t = get_raw_file_type(f);
		// ignore . entries if specified
		if (ignore_dot && t != FileType::Dir && is_dot_path(f))
			continue;
		if (t == FileType::Reg || t == FileType::Symlink)
			l.push_back(f);
	}
	return l;
}

std::vector<std::string> load_flist_file(const std::string& flist_file) {
	std::ifstream ifs;
	ifs.exceptions(std::ofstream::failbit | std::ifstream::badbit);
	ifs.open(flist_file);
	ifs.exceptions(std::ifstream::badbit); // getline sets failbit on EOF

	std::vector<std::string> fl;
	std::string s;
	while (std::getline(ifs, s))
		fl.push_back(s);
	return fl;
}

int create_flist_file(const std::vector<std::string>& input,
	const std::string& flist_file, bool ignore_dot, bool force) {
	if (path_exists(flist_file)) {
		if (force) {
			if (get_raw_file_type(flist_file) != FileType::Reg)
				return -EINVAL;
			std::error_code ec;
			if (std::filesystem::remove(flist_file, ec)) {
				std::cout << "Removed " << flist_file
					<< std::endl;
			} else {
				std::cout << "Failed to remove " << flist_file
					<< std::endl;
				return -ec.value();
			}
		} else {
			return -EEXIST;
		}
	}

	std::vector<std::string> fl;
	for (const auto& f : input) {
		auto v = init_flist(f, ignore_dot);
		std::cout << v.size() << " files scanned from " << f << std::endl;
		for (const auto& s : v)
			fl.push_back(s);
	}
	std::sort(fl.begin(), fl.end());

	std::ofstream ofs;
	ofs.exceptions(std::ofstream::failbit | std::ofstream::badbit);
	ofs.open(flist_file);

	for (const auto& s : fl)
		ofs << s << std::endl;
	ofs.flush();
	return 0;
}
