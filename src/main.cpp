// main.cpp

#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <cstdio>
#include <boost/json.hpp>

namespace json = boost::json;

// Runs a shell command and captures its output
std::string run_command(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) throw std::runtime_error("popen() failed");
    while (fgets(buffer.data(), buffer.size(), pipe)) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}

// Pretty print function from Boost documentation
// https://www.boost.org/doc/libs/latest/libs/json/doc/html/examples.html#pretty
void pretty_print( std::ostream& os, json::value const& jv, std::string* indent = nullptr ) {
    std::string indent_;
    if(!indent)
        indent = &indent_;
    switch(jv.kind()) {
    case json::kind::object: {
        os << "{\n";
        indent->append(4, ' ');
        auto const& obj = jv.get_object();
        if(!obj.empty()) {
            auto it = obj.begin();
            for(;;) {
                os << *indent << json::serialize(it->key()) << " : ";
                pretty_print(os, it->value(), indent);
                if(++it == obj.end())
                    break;
                os << ",\n";
            }
        }
        os << "\n";
        indent->resize(indent->size() - 4);
        os << *indent << "}";
        break;
    }
    case json::kind::array: {
        os << "[\n";
        indent->append(4, ' ');
        auto const& arr = jv.get_array();
        if(!arr.empty()) {
            auto it = arr.begin();
            for(;;) {
                os << *indent;
                pretty_print( os, *it, indent);
                if(++it == arr.end())
                    break;
                os << ",\n";
            }
        }
        os << "\n";
        indent->resize(indent->size() - 4);
        os << *indent << "]";
        break;
    }
    case json::kind::string: {
        os << json::serialize(jv.get_string());
        break;
    }
    case json::kind::uint64:
    case json::kind::int64:
    case json::kind::double_:
        os << jv;
        break;
    case json::kind::bool_:
        if(jv.get_bool())
            os << "true";
        else
            os << "false";
        break;

    case json::kind::null:
        os << "null";
        break;
    }

    if(indent->empty())
        os << "\n";
}

// Parses a JSON string into a Boost.JSON object
json::object parse_json(const std::string& raw) {
    if (raw.empty() || raw[0] != '{') {
        throw std::runtime_error("Invalid or empty JSON input: " + raw);
    }
    return json::parse(raw).as_object();
}

// Gets the block hash for a given block height
std::string get_block_hash(int height) {
    std::ostringstream cmd;
    cmd << "bitcoin-cli getblockhash " << height;
    return run_command(cmd.str());
}

// Gets the block as a JSON string for a given block hash
std::string get_block(const std::string& hash) {
    std::ostringstream cmd;
    cmd << "bitcoin-cli getblock " << hash;
    return run_command(cmd.str());
}

int main(int argc, char** argv) {
    int start_height = 0; // We will treat this as meaning start at the tip
    int max_blocks = 1;

    // Parse command line argument for number of days
    if (argc > 2) {
        std::cerr << "Usage: " << argv[0] << " [start_height] [max_blocks]\n";
        return 1;
    }
    if (argc > 1) start_height = std::stoi(argv[1]);
    if (argc > 2) max_blocks = std::stoi(argv[2]);

    // Get current blockchain info
    auto info = parse_json(run_command("bitcoin-cli getblockchaininfo"));
    int current_height = info["blocks"].as_int64();

    // If for some reason the start_height is set higher than the current_height,
    // we will silently fail over to the current_height.
    if ((start_height > 0) && (start_height < current_height))
        current_height = start_height;

    // Get current block
    auto head_hash = get_block_hash(current_height);
    auto block = parse_json(get_block(head_hash));

    for (int i = 0; i < max_blocks; ++i) {
        pretty_print(std::cout, block);
        break;

        // Get previous block
        if (auto* val = block.if_contains("previousblockhash")) {
            auto hash = val->as_string().c_str();
            block = parse_json(get_block(hash));
        } else
            break;
    }

    return 0;
}

