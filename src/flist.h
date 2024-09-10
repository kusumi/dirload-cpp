#ifndef SRC_FLIST_H_
#define SRC_FLIST_H_

#include <vector>
#include <string>

std::vector<std::string> init_flist(const std::string&, bool);
std::vector<std::string> load_flist_file(const std::string&);
int create_flist_file(const std::vector<std::string>&, const std::string&,
	bool, bool);
#endif // SRC_FLIST_H_
