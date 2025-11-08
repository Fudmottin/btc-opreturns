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

void write_to_file(const std::string& filename, const std::string& hex) {
    std::ofstream out(filename, std::ios::out | std::ios::app);
    if (!out) {
        std::cerr << "Error: cannot open " << filename << " for writing.\n";
        return;
    }

    out << hex << '\n';
    if (!out)
        std::cerr << "Error: failed to write to " << filename << ".\n";
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

// Gets the transaction as a JSON object for a given transaction hash
json::object get_transaction(const std::string& hash) {
    std::ostringstream cmd;
    cmd << "bitcoin-cli getrawtransaction " << hash << " 1";
    return parse_json(run_command(cmd.str()));
}

// The hex data includes the OP_RETURN code and length metadata so trim it
std::string trim_op_return_prefix(const std::string& hex) {
    if (hex.size() < 2 || hex.substr(0, 2) != "6a")
        return hex; // not OP_RETURN

    size_t pos = 2; // skip 6a

    if (hex.size() < 4)
        return "";

    const std::string op = hex.substr(pos, 2);
    pos += 2;

    if (op == "4c") pos += 2;        // PUSHDATA1: 1 byte length
    else if (op == "4d") pos += 4;   // PUSHDATA2: 2 bytes length
    else if (op == "4e") pos += 8;   // PUSHDATA4: 4 bytes length
    else pos -= 2;                   // immediate data length < 76, no prefix

    if (pos >= hex.size())
        return "";

    return hex.substr(pos);
}

// Look for OP_RETURNs and store them in a vector if they exist.
std::vector<std::string> process_transaction(const json::object& tx) {
    std::vector<std::string> results;

    if (!tx.contains("vout")) {
        std::cerr << "Transaction has no outputs.\n";
        return results;
    }

    const auto& vout = tx.at("vout").as_array();

    for (const auto& out : vout) {
        const auto& obj = out.as_object();

        if (!obj.contains("scriptPubKey"))
            continue;

        const auto& script = obj.at("scriptPubKey").as_object();

        if (!script.contains("asm"))
            continue;

        const std::string& asm_field = script.at("asm").as_string().c_str();

        if (asm_field.rfind("OP_RETURN", 0) == 0) {
            // OP_RETURN found
            if (script.contains("hex")) {
                results.push_back(trim_op_return_prefix(script.at("hex").as_string().c_str()));
            } else {
                // fallback: take entire asm field
                results.push_back(asm_field);
            }
        }
    }

    return results;
}

// Process each transaction
void process_transactions(const json::value& txs) {
    int i = 0;
    for (auto tx : txs.as_array()) {
        if (i++ == 0)
            continue; // skip coinbase

        auto tx_json = get_transaction(tx.as_string().c_str());
        auto op_returns = process_transaction(tx_json);

        if (op_returns.size() > 0) {
/*            std::cout << "Transaction " << tx.as_string()
                      << " has " << op_returns.size() << " OP_RETURN outputs.\r";
*/
            for (const auto& data : op_returns) {
                if (data.size() > 160) {
                    std::string filename {tx.as_string().c_str()};
                    filename += ".txt";
                    write_to_file(filename, data);
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    int start_height = 0; // We will treat this as meaning start at the tip
    int max_blocks = 1;

    // Parse command line argument for number of days
    if (argc > 3) {
        std::cerr << "Usage: " << argv[0] << " [start_height] [max_blocks]\n";
        return 1;
    }
    if (argc > 1) start_height = std::stoi(argv[1]);

    // Get current blockchain info
    auto info = parse_json(run_command("bitcoin-cli getblockchaininfo"));
    int current_height = info["blocks"].as_int64();

    // If for some reason the start_height is set higher than the current_height,
    // we will silently fail over to the current_height.
    if ((start_height > 0) && (start_height < current_height))
        current_height = start_height;

    if (argc > 2) max_blocks = std::stoi(argv[2]);
    else max_blocks = current_height;

    // Get current block
    auto head_hash = get_block_hash(current_height);
    auto block = parse_json(get_block(head_hash));

    for (int i = 0; i < max_blocks; ++i) {
        int number_of_transactions = block["nTx"].as_int64();
        auto transactions = block["tx"];

        std::cout << "Processing block " << current_height-- << " with " << number_of_transactions << " transactions...\n";
        process_transactions(transactions);

        // Get previous block
        if (auto* val = block.if_contains("previousblockhash")) {
            auto hash = val->as_string().c_str();
            block = parse_json(get_block(hash));
        } else
            break;
    }

    return 0;
}

