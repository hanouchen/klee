#ifndef KLEE_COMMAND_H
#define KLEE_COMMAND_H
#include <iostream>
#include <assert.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <functional>
#include <sstream>
#include <algorithm>
#include <iterator>

namespace klee {


class CommandBuffer {
public:
    CommandBuffer(char *line) : tokens() {
        std::istringstream iss(line);
        std::copy(std::istream_iterator<std::string>(iss),
                  std::istream_iterator<std::string>(),
                  back_inserter(tokens));
    }

    template<typename T>
    const T& search(std::pair<const char*, T> *list) {
        std::pair<const char*, T> *item;
        for (item = list; item->first; ++item) {
            if (strcmp(nextToken().c_str(), item->first) == 0) {
                if (!tokens.empty()) tokens.erase(tokens.begin());
                break;
            }
        }
        return item->second;
    }

    std::string nextToken() { 
        return endOfLine() ? "" : tokens.front(); 
    }

    bool endOfLine() { return tokens.empty(); }

private:
    std::vector<std::string> tokens;

};

}
#endif